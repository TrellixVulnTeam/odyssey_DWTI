/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "PolymorphicAccess.h"

#if ENABLE(JIT)

#include "BinarySwitch.h"
#include "CCallHelpers.h"
#include "CodeBlock.h"
#include "GetterSetter.h"
#include "Heap.h"
#include "JITOperations.h"
#include "JSCInlines.h"
#include "LinkBuffer.h"
#include "ScratchRegisterAllocator.h"
#include "StructureStubClearingWatchpoint.h"
#include "StructureStubInfo.h"
#include <wtf/CommaPrinter.h>
#include <wtf/ListDump.h>

namespace JSC {

static const bool verbose = false;

struct AccessGenerationState {
    CCallHelpers* jit { nullptr };
    ScratchRegisterAllocator* allocator;
    size_t numberOfPaddingBytes { 0 };
    PolymorphicAccess* access { nullptr };
    StructureStubInfo* stubInfo { nullptr };
    CCallHelpers::JumpList success;
    CCallHelpers::JumpList failAndRepatch;
    CCallHelpers::JumpList failAndIgnore;
    GPRReg baseGPR { InvalidGPRReg };
    JSValueRegs valueRegs;
    GPRReg scratchGPR { InvalidGPRReg };
    Vector<std::function<void(LinkBuffer&)>> callbacks;
    const Identifier* ident;
    std::unique_ptr<WatchpointsOnStructureStubInfo> watchpoints;
    Vector<WriteBarrier<JSCell>> weakReferences;

    Watchpoint* addWatchpoint(const ObjectPropertyCondition& condition = ObjectPropertyCondition())
    {
        return WatchpointsOnStructureStubInfo::ensureReferenceAndAddWatchpoint(
            watchpoints, jit->codeBlock(), stubInfo, condition);
    }

    void restoreScratch()
    {
        allocator->restoreReusedRegistersByPopping(*jit, numberOfPaddingBytes);
    }

    void succeed()
    {
        restoreScratch();
        success.append(jit->jump());
    }
};

AccessCase::AccessCase()
{
}

std::unique_ptr<AccessCase> AccessCase::get(
    VM& vm, JSCell* owner, AccessType type, PropertyOffset offset, Structure* structure,
    const ObjectPropertyConditionSet& conditionSet, bool viaProxy, WatchpointSet* additionalSet,
    PropertySlot::GetValueFunc customGetter, JSObject* customSlotBase)
{
    std::unique_ptr<AccessCase> result(new AccessCase());

    result->m_type = type;
    result->m_offset = offset;
    result->m_structure.set(vm, owner, structure);
    result->m_conditionSet = conditionSet;

    if (viaProxy || additionalSet || result->doesCalls() || customGetter || customSlotBase) {
        result->m_rareData = std::make_unique<RareData>();
        result->m_rareData->viaProxy = viaProxy;
        result->m_rareData->additionalSet = additionalSet;
        result->m_rareData->customAccessor.getter = customGetter;
        result->m_rareData->customSlotBase.setMayBeNull(vm, owner, customSlotBase);
    }

    return result;
}

std::unique_ptr<AccessCase> AccessCase::replace(
    VM& vm, JSCell* owner, Structure* structure, PropertyOffset offset)
{
    std::unique_ptr<AccessCase> result(new AccessCase());

    result->m_type = Replace;
    result->m_offset = offset;
    result->m_structure.set(vm, owner, structure);

    return result;
}

std::unique_ptr<AccessCase> AccessCase::transition(
    VM& vm, JSCell* owner, Structure* oldStructure, Structure* newStructure, PropertyOffset offset,
    const ObjectPropertyConditionSet& conditionSet)
{
    RELEASE_ASSERT(oldStructure == newStructure->previousID());

    // Skip optimizing the case where we need a realloc, if we don't have
    // enough registers to make it happen.
    if (GPRInfo::numberOfRegisters < 6
        && oldStructure->outOfLineCapacity() != newStructure->outOfLineCapacity()
        && oldStructure->outOfLineCapacity()) {
        return nullptr;
    }

    // Skip optimizing the case where we need realloc, and the structure has
    // indexing storage.
    // FIXME: We shouldn't skip this! Implement it!
    // https://bugs.webkit.org/show_bug.cgi?id=130914
    if (oldStructure->couldHaveIndexingHeader())
        return nullptr;

    std::unique_ptr<AccessCase> result(new AccessCase());

    result->m_type = Transition;
    result->m_offset = offset;
    result->m_structure.set(vm, owner, newStructure);
    result->m_conditionSet = conditionSet;

    return result;
}

std::unique_ptr<AccessCase> AccessCase::setter(
    VM& vm, JSCell* owner, AccessType type, Structure* structure, PropertyOffset offset,
    const ObjectPropertyConditionSet& conditionSet, PutPropertySlot::PutValueFunc customSetter,
    JSObject* customSlotBase)
{
    std::unique_ptr<AccessCase> result(new AccessCase());

    result->m_type = type;
    result->m_offset = offset;
    result->m_structure.set(vm, owner, structure);
    result->m_conditionSet = conditionSet;
    result->m_rareData = std::make_unique<RareData>();
    result->m_rareData->customAccessor.setter = customSetter;
    result->m_rareData->customSlotBase.setMayBeNull(vm, owner, customSlotBase);

    return result;
}

std::unique_ptr<AccessCase> AccessCase::in(
    VM& vm, JSCell* owner, AccessType type, Structure* structure,
    const ObjectPropertyConditionSet& conditionSet)
{
    std::unique_ptr<AccessCase> result(new AccessCase());

    result->m_type = type;
    result->m_structure.set(vm, owner, structure);
    result->m_conditionSet = conditionSet;

    return result;
}

std::unique_ptr<AccessCase> AccessCase::getLength(VM&, JSCell*, AccessType type)
{
    std::unique_ptr<AccessCase> result(new AccessCase());

    result->m_type = type;

    return result;
}

AccessCase::~AccessCase()
{
}

std::unique_ptr<AccessCase> AccessCase::fromStructureStubInfo(
    VM& vm, JSCell* owner, StructureStubInfo& stubInfo)
{
    switch (stubInfo.cacheType) {
    case CacheType::GetByIdSelf:
        return get(
            vm, owner, Load, stubInfo.u.byIdSelf.offset,
            stubInfo.u.byIdSelf.baseObjectStructure.get());

    case CacheType::PutByIdReplace:
        return replace(
            vm, owner, stubInfo.u.byIdSelf.baseObjectStructure.get(), stubInfo.u.byIdSelf.offset);

    default:
        return nullptr;
    }
}

std::unique_ptr<AccessCase> AccessCase::clone() const
{
    std::unique_ptr<AccessCase> result(new AccessCase());
    result->m_type = m_type;
    result->m_offset = m_offset;
    result->m_structure = m_structure;
    result->m_conditionSet = m_conditionSet;
    if (RareData* rareData = m_rareData.get()) {
        result->m_rareData = std::make_unique<RareData>();
        result->m_rareData->viaProxy = rareData->viaProxy;
        result->m_rareData->additionalSet = rareData->additionalSet;
        // NOTE: We don't copy the callLinkInfo, since that's created during code generation.
        result->m_rareData->customAccessor.opaque = rareData->customAccessor.opaque;
        result->m_rareData->customSlotBase = rareData->customSlotBase;
    }
    return result;
}

bool AccessCase::guardedByStructureCheck() const
{
    if (viaProxy())
        return false;

    switch (m_type) {
    case ArrayLength:
    case StringLength:
        return false;
    default:
        return true;
    }
}

JSObject* AccessCase::alternateBase() const
{
    if (customSlotBase())
        return customSlotBase();
    return conditionSet().slotBaseCondition().object();
}

bool AccessCase::couldStillSucceed() const
{
    return m_conditionSet.structuresEnsureValidityAssumingImpurePropertyWatchpoint();
}

bool AccessCase::canReplace(const AccessCase& other)
{
    // We could do a lot better here, but for now we just do something obvious.

    if (!guardedByStructureCheck() || !other.guardedByStructureCheck()) {
        // FIXME: Implement this!
        return false;
    }

    return structure() == other.structure();
}

void AccessCase::dump(PrintStream& out) const
{
    out.print(m_type, ":(");

    CommaPrinter comma;

    if (m_type == Transition)
        out.print(comma, "structure = ", pointerDump(structure()), " -> ", pointerDump(newStructure()));
    else if (m_structure)
        out.print(comma, "structure = ", pointerDump(m_structure.get()));

    if (isValidOffset(m_offset))
        out.print(comma, "offset = ", m_offset);
    if (!m_conditionSet.isEmpty())
        out.print(comma, "conditions = ", m_conditionSet);

    if (RareData* rareData = m_rareData.get()) {
        if (rareData->viaProxy)
            out.print(comma, "viaProxy = ", rareData->viaProxy);
        if (rareData->additionalSet)
            out.print(comma, "additionalSet = ", RawPointer(rareData->additionalSet.get()));
        if (rareData->callLinkInfo)
            out.print(comma, "callLinkInfo = ", RawPointer(rareData->callLinkInfo.get()));
        if (rareData->customAccessor.opaque)
            out.print(comma, "customAccessor = ", RawPointer(rareData->customAccessor.opaque));
        if (rareData->customSlotBase)
            out.print(comma, "customSlotBase = ", RawPointer(rareData->customSlotBase.get()));
    }

    out.print(")");
}

bool AccessCase::visitWeak(VM& vm) const
{
    if (m_structure && !Heap::isMarked(m_structure.get()))
        return false;
    if (!m_conditionSet.areStillLive())
        return false;
    if (m_rareData) {
        if (m_rareData->callLinkInfo)
            m_rareData->callLinkInfo->visitWeak(vm);
        if (m_rareData->customSlotBase && !Heap::isMarked(m_rareData->customSlotBase.get()))
            return false;
    }
    return true;
}

void AccessCase::generateWithGuard(
    AccessGenerationState& state, CCallHelpers::JumpList& fallThrough)
{
    CCallHelpers& jit = *state.jit;

    switch (m_type) {
    case ArrayLength: {
        ASSERT(!viaProxy());
        jit.load8(CCallHelpers::Address(state.baseGPR, JSCell::indexingTypeOffset()), state.scratchGPR);
        fallThrough.append(
            jit.branchTest32(
                CCallHelpers::Zero, state.scratchGPR, CCallHelpers::TrustedImm32(IsArray)));
        fallThrough.append(
            jit.branchTest32(
                CCallHelpers::Zero, state.scratchGPR, CCallHelpers::TrustedImm32(IndexingShapeMask)));
        break;
    }

    case StringLength: {
        ASSERT(!viaProxy());
        fallThrough.append(
            jit.branch8(
                CCallHelpers::NotEqual,
                CCallHelpers::Address(state.baseGPR, JSCell::typeInfoTypeOffset()),
                CCallHelpers::TrustedImm32(StringType)));
        break;
    }

    default: {
        if (viaProxy()) {
            fallThrough.append(
                jit.branch8(
                    CCallHelpers::NotEqual,
                    CCallHelpers::Address(state.baseGPR, JSCell::typeInfoTypeOffset()),
                    CCallHelpers::TrustedImm32(PureForwardingProxyType)));

            jit.loadPtr(
                CCallHelpers::Address(state.baseGPR, JSProxy::targetOffset()),
                state.scratchGPR);

            fallThrough.append(
                jit.branchStructure(
                    CCallHelpers::NotEqual,
                    CCallHelpers::Address(state.scratchGPR, JSCell::structureIDOffset()),
                    structure()));
        } else {
            fallThrough.append(
                jit.branchStructure(
                    CCallHelpers::NotEqual,
                    CCallHelpers::Address(state.baseGPR, JSCell::structureIDOffset()),
                    structure()));
        }
        break;
    } };

    generate(state);
}

void AccessCase::generate(AccessGenerationState& state)
{
    if (verbose)
        dataLog("Generating code for: ", *this, "\n");
    
    CCallHelpers& jit = *state.jit;
    VM& vm = *jit.vm();
    CodeBlock* codeBlock = jit.codeBlock();
    StructureStubInfo& stubInfo = *state.stubInfo;
    const Identifier& ident = *state.ident;
    JSValueRegs valueRegs = state.valueRegs;
    GPRReg baseGPR = state.baseGPR;
    GPRReg scratchGPR = state.scratchGPR;

    ASSERT(m_conditionSet.structuresEnsureValidityAssumingImpurePropertyWatchpoint());

    if ((structure() && structure()->needImpurePropertyWatchpoint())
        || m_conditionSet.needImpurePropertyWatchpoint())
        vm.registerWatchpointForImpureProperty(ident, state.addWatchpoint());

    if (additionalSet())
        additionalSet()->add(state.addWatchpoint());

    for (const ObjectPropertyCondition& condition : m_conditionSet) {
        Structure* structure = condition.object()->structure();

        if (condition.isWatchableAssumingImpurePropertyWatchpoint()) {
            structure->addTransitionWatchpoint(state.addWatchpoint(condition));
            continue;
        }

        if (!condition.structureEnsuresValidityAssumingImpurePropertyWatchpoint(structure)) {
            dataLog("This condition is no longer met: ", condition, "\n");
            RELEASE_ASSERT_NOT_REACHED();
        }

        // We will emit code that has a weak reference that isn't otherwise listed anywhere.
        state.weakReferences.append(WriteBarrier<JSCell>(vm, codeBlock->ownerExecutable(), structure));
        
        jit.move(CCallHelpers::TrustedImmPtr(condition.object()), scratchGPR);
        state.failAndRepatch.append(
            jit.branchStructure(
                CCallHelpers::NotEqual,
                CCallHelpers::Address(scratchGPR, JSCell::structureIDOffset()),
                structure));
    }

    switch (m_type) {
    case InHit:
    case InMiss:
        jit.boxBooleanPayload(m_type == InHit, valueRegs.payloadGPR());
        state.succeed();
        return;

    case Miss:
        jit.moveTrustedValue(jsUndefined(), valueRegs);
        state.succeed();
        return;

    case Load:
    case Getter:
    case Setter:
    case CustomGetter:
    case CustomSetter: {
        if (isValidOffset(m_offset)) {
            Structure* currStructure;
            if (m_conditionSet.isEmpty())
                currStructure = structure();
            else
                currStructure = m_conditionSet.slotBaseCondition().object()->structure();
            currStructure->startWatchingPropertyForReplacements(vm, offset());
        }

        GPRReg baseForGetGPR;
        if (viaProxy()) {
            baseForGetGPR = valueRegs.payloadGPR();
            jit.loadPtr(
                CCallHelpers::Address(baseGPR, JSProxy::targetOffset()),
                baseForGetGPR);
        } else
            baseForGetGPR = baseGPR;

        GPRReg baseForAccessGPR;
        if (!m_conditionSet.isEmpty()) {
            jit.move(
                CCallHelpers::TrustedImmPtr(alternateBase()),
                scratchGPR);
            baseForAccessGPR = scratchGPR;
        } else
            baseForAccessGPR = baseForGetGPR;

        GPRReg loadedValueGPR = InvalidGPRReg;
        if (m_type != CustomGetter && m_type != CustomSetter) {
            if (m_type == Load)
                loadedValueGPR = valueRegs.payloadGPR();
            else
                loadedValueGPR = scratchGPR;

            GPRReg storageGPR;
            if (isInlineOffset(m_offset))
                storageGPR = baseForAccessGPR;
            else {
                jit.loadPtr(
                    CCallHelpers::Address(baseForAccessGPR, JSObject::butterflyOffset()),
                    loadedValueGPR);
                storageGPR = loadedValueGPR;
            }

#if USE(JSVALUE64)
            jit.load64(
                CCallHelpers::Address(storageGPR, offsetRelativeToBase(m_offset)), loadedValueGPR);
#else
            if (m_type == Load) {
                jit.load32(
                    CCallHelpers::Address(storageGPR, offsetRelativeToBase(m_offset) + TagOffset),
                    valueRegs.tagGPR());
            }
            jit.load32(
                CCallHelpers::Address(storageGPR, offsetRelativeToBase(m_offset) + PayloadOffset),
                loadedValueGPR);
#endif
        }

        // Stuff for custom getters.
        CCallHelpers::Call operationCall;
        CCallHelpers::Call handlerCall;

        // Stuff for JS getters.
        CCallHelpers::DataLabelPtr addressOfLinkFunctionCheck;
        CCallHelpers::Call fastPathCall;
        CCallHelpers::Call slowPathCall;

        CCallHelpers::Jump success;
        CCallHelpers::Jump fail;
        if (m_type != Load && m_type != Miss) {
            // Need to make sure that whenever this call is made in the future, we remember the
            // place that we made it from.
            jit.store32(
                CCallHelpers::TrustedImm32(stubInfo.callSiteIndex.bits()),
                CCallHelpers::tagFor(static_cast<VirtualRegister>(JSStack::ArgumentCount)));

            if (m_type == Getter || m_type == Setter) {
                // Create a JS call using a JS call inline cache. Assume that:
                //
                // - SP is aligned and represents the extent of the calling compiler's stack usage.
                //
                // - FP is set correctly (i.e. it points to the caller's call frame header).
                //
                // - SP - FP is an aligned difference.
                //
                // - Any byte between FP (exclusive) and SP (inclusive) could be live in the calling
                //   code.
                //
                // Therefore, we temporarily grow the stack for the purpose of the call and then
                // shrink it after.

                RELEASE_ASSERT(!m_rareData->callLinkInfo);
                m_rareData->callLinkInfo = std::make_unique<CallLinkInfo>();
                
                // FIXME: If we generated a polymorphic call stub that jumped back to the getter
                // stub, which then jumped back to the main code, then we'd have a reachability
                // situation that the GC doesn't know about. The GC would ensure that the polymorphic
                // call stub stayed alive, and it would ensure that the main code stayed alive, but
                // it wouldn't know that the getter stub was alive. Ideally JIT stub routines would
                // be GC objects, and then we'd be able to say that the polymorphic call stub has a
                // reference to the getter stub.
                // https://bugs.webkit.org/show_bug.cgi?id=148914
                m_rareData->callLinkInfo->disallowStubs();
                
                m_rareData->callLinkInfo->setUpCall(
                    CallLinkInfo::Call, stubInfo.codeOrigin, loadedValueGPR);

                CCallHelpers::JumpList done;

                // There is a "this" argument.
                unsigned numberOfParameters = 1;
                // ... and a value argument if we're calling a setter.
                if (m_type == Setter)
                    numberOfParameters++;

                // Get the accessor; if there ain't one then the result is jsUndefined().
                if (m_type == Setter) {
                    jit.loadPtr(
                        CCallHelpers::Address(loadedValueGPR, GetterSetter::offsetOfSetter()),
                        loadedValueGPR);
                } else {
                    jit.loadPtr(
                        CCallHelpers::Address(loadedValueGPR, GetterSetter::offsetOfGetter()),
                        loadedValueGPR);
                }

                CCallHelpers::Jump returnUndefined = jit.branchTestPtr(
                    CCallHelpers::Zero, loadedValueGPR);

                unsigned numberOfRegsForCall = JSStack::CallFrameHeaderSize + numberOfParameters;

                unsigned numberOfBytesForCall =
                    numberOfRegsForCall * sizeof(Register) + sizeof(CallerFrameAndPC);

                unsigned alignedNumberOfBytesForCall =
                    WTF::roundUpToMultipleOf(stackAlignmentBytes(), numberOfBytesForCall);

                jit.subPtr(
                    CCallHelpers::TrustedImm32(alignedNumberOfBytesForCall),
                    CCallHelpers::stackPointerRegister);

                CCallHelpers::Address calleeFrame = CCallHelpers::Address(
                    CCallHelpers::stackPointerRegister,
                    -static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC)));

                jit.store32(
                    CCallHelpers::TrustedImm32(numberOfParameters),
                    calleeFrame.withOffset(JSStack::ArgumentCount * sizeof(Register) + PayloadOffset));

                jit.storeCell(
                    loadedValueGPR, calleeFrame.withOffset(JSStack::Callee * sizeof(Register)));

                jit.storeCell(
                    baseForGetGPR,
                    calleeFrame.withOffset(virtualRegisterForArgument(0).offset() * sizeof(Register)));

                if (m_type == Setter) {
                    jit.storeValue(
                        valueRegs,
                        calleeFrame.withOffset(
                            virtualRegisterForArgument(1).offset() * sizeof(Register)));
                }

                CCallHelpers::Jump slowCase = jit.branchPtrWithPatch(
                    CCallHelpers::NotEqual, loadedValueGPR, addressOfLinkFunctionCheck,
                    CCallHelpers::TrustedImmPtr(0));

                fastPathCall = jit.nearCall();

                jit.addPtr(
                    CCallHelpers::TrustedImm32(alignedNumberOfBytesForCall),
                    CCallHelpers::stackPointerRegister);
                if (m_type == Getter)
                    jit.setupResults(valueRegs);

                done.append(jit.jump());
                slowCase.link(&jit);

                jit.move(loadedValueGPR, GPRInfo::regT0);
#if USE(JSVALUE32_64)
                // We *always* know that the getter/setter, if non-null, is a cell.
                jit.move(CCallHelpers::TrustedImm32(JSValue::CellTag), GPRInfo::regT1);
#endif
                jit.move(CCallHelpers::TrustedImmPtr(m_rareData->callLinkInfo.get()), GPRInfo::regT2);
                slowPathCall = jit.nearCall();

                jit.addPtr(
                    CCallHelpers::TrustedImm32(alignedNumberOfBytesForCall),
                    CCallHelpers::stackPointerRegister);
                if (m_type == Getter)
                    jit.setupResults(valueRegs);

                done.append(jit.jump());
                returnUndefined.link(&jit);

                if (m_type == Getter)
                    jit.moveTrustedValue(jsUndefined(), valueRegs);

                done.link(&jit);

                jit.addPtr(
                    CCallHelpers::TrustedImm32(
                        jit.codeBlock()->stackPointerOffset() * sizeof(Register)),
                    GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);

                state.callbacks.append(
                    [=, &vm] (LinkBuffer& linkBuffer) {
                        m_rareData->callLinkInfo->setCallLocations(
                            linkBuffer.locationOfNearCall(slowPathCall),
                            linkBuffer.locationOf(addressOfLinkFunctionCheck),
                            linkBuffer.locationOfNearCall(fastPathCall));

                        linkBuffer.link(
                            slowPathCall,
                            CodeLocationLabel(vm.getCTIStub(linkCallThunkGenerator).code()));
                    });
            } else {
                // getter: EncodedJSValue (*GetValueFunc)(ExecState*, JSObject* slotBase, EncodedJSValue thisValue, PropertyName);
                // setter: void (*PutValueFunc)(ExecState*, JSObject* base, EncodedJSValue thisObject, EncodedJSValue value);
#if USE(JSVALUE64)
                if (m_type == CustomGetter) {
                    jit.setupArgumentsWithExecState(
                        baseForAccessGPR, baseForGetGPR,
                        CCallHelpers::TrustedImmPtr(ident.impl()));
                } else
                    jit.setupArgumentsWithExecState(baseForAccessGPR, baseForGetGPR, valueRegs.gpr());
#else
                if (m_type == CustomGetter) {
                    jit.setupArgumentsWithExecState(
                        baseForAccessGPR, baseForGetGPR,
                        CCallHelpers::TrustedImm32(JSValue::CellTag),
                        CCallHelpers::TrustedImmPtr(ident.impl()));
                } else {
                    jit.setupArgumentsWithExecState(
                        baseForAccessGPR, baseForGetGPR,
                        CCallHelpers::TrustedImm32(JSValue::CellTag),
                        valueRegs.payloadGPR(), valueRegs.tagGPR());
                }
#endif
                jit.storePtr(GPRInfo::callFrameRegister, &vm.topCallFrame);

                operationCall = jit.call();
                if (m_type == CustomGetter)
                    jit.setupResults(valueRegs);
                CCallHelpers::Jump noException =
                    jit.emitExceptionCheck(CCallHelpers::InvertedExceptionCheck);

                jit.copyCalleeSavesToVMCalleeSavesBuffer();
                jit.setupArguments(CCallHelpers::TrustedImmPtr(&vm), GPRInfo::callFrameRegister);
                handlerCall = jit.call();
                jit.jumpToExceptionHandler();
            
                noException.link(&jit);

                state.callbacks.append(
                    [=] (LinkBuffer& linkBuffer) {
                        linkBuffer.link(operationCall, FunctionPtr(m_rareData->customAccessor.opaque));
                        linkBuffer.link(handlerCall, lookupExceptionHandler);
                    });
            }
        }
        state.succeed();
        return;
    }

    case Replace: {
        if (InferredType* type = structure()->inferredTypeFor(ident.impl())) {
            if (verbose)
                dataLog("Have type: ", type->descriptor(), "\n");
            state.failAndRepatch.append(
                jit.branchIfNotType(
                    valueRegs, scratchGPR, type->descriptor(), CCallHelpers::DoNotHaveTagRegisters));
        } else if (verbose)
            dataLog("Don't have type.\n");
        
        if (isInlineOffset(m_offset)) {
            jit.storeValue(
                valueRegs,
                CCallHelpers::Address(
                    baseGPR,
                    JSObject::offsetOfInlineStorage() +
                    offsetInInlineStorage(m_offset) * sizeof(JSValue)));
        } else {
            jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
            jit.storeValue(
                valueRegs,
                CCallHelpers::Address(
                    scratchGPR, offsetInButterfly(m_offset) * sizeof(JSValue)));
        }
        state.succeed();
        return;
    }

    case Transition: {
        // AccessCase::transition() should have returned null.
        RELEASE_ASSERT(GPRInfo::numberOfRegisters >= 6 || !structure()->outOfLineCapacity() || structure()->outOfLineCapacity() == newStructure()->outOfLineCapacity());
        RELEASE_ASSERT(!structure()->couldHaveIndexingHeader());

        if (InferredType* type = newStructure()->inferredTypeFor(ident.impl())) {
            if (verbose)
                dataLog("Have type: ", type->descriptor(), "\n");
            state.failAndRepatch.append(
                jit.branchIfNotType(
                    valueRegs, scratchGPR, type->descriptor(), CCallHelpers::DoNotHaveTagRegisters));
        } else if (verbose)
            dataLog("Don't have type.\n");
        
        CCallHelpers::JumpList slowPath;

        ScratchRegisterAllocator allocator(stubInfo.patch.usedRegisters);
        allocator.lock(baseGPR);
        allocator.lock(valueRegs);
        allocator.lock(scratchGPR);

        GPRReg scratchGPR2 = allocator.allocateScratchGPR();
        GPRReg scratchGPR3;
        if (newStructure()->outOfLineCapacity() != structure()->outOfLineCapacity()
            && structure()->outOfLineCapacity())
            scratchGPR3 = allocator.allocateScratchGPR();
        else
            scratchGPR3 = InvalidGPRReg;

        size_t numberOfPaddingBytes = allocator.preserveReusedRegistersByPushing(jit);

        ASSERT(structure()->transitionWatchpointSetHasBeenInvalidated());

        bool scratchGPRHasStorage = false;

        if (newStructure()->outOfLineCapacity() != structure()->outOfLineCapacity()) {
            size_t newSize = newStructure()->outOfLineCapacity() * sizeof(JSValue);
            CopiedAllocator* copiedAllocator = &vm.heap.storageAllocator();

            if (!structure()->outOfLineCapacity()) {
                jit.loadPtr(&copiedAllocator->m_currentRemaining, scratchGPR);
                slowPath.append(
                    jit.branchSubPtr(
                        CCallHelpers::Signed, CCallHelpers::TrustedImm32(newSize), scratchGPR));
                jit.storePtr(scratchGPR, &copiedAllocator->m_currentRemaining);
                jit.negPtr(scratchGPR);
                jit.addPtr(
                    CCallHelpers::AbsoluteAddress(&copiedAllocator->m_currentPayloadEnd), scratchGPR);
                jit.addPtr(CCallHelpers::TrustedImm32(sizeof(JSValue)), scratchGPR);
            } else {
                size_t oldSize = structure()->outOfLineCapacity() * sizeof(JSValue);
                ASSERT(newSize > oldSize);
            
                jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR3);
                jit.loadPtr(&copiedAllocator->m_currentRemaining, scratchGPR);
                slowPath.append(
                    jit.branchSubPtr(
                        CCallHelpers::Signed, CCallHelpers::TrustedImm32(newSize), scratchGPR));
                jit.storePtr(scratchGPR, &copiedAllocator->m_currentRemaining);
                jit.negPtr(scratchGPR);
                jit.addPtr(
                    CCallHelpers::AbsoluteAddress(&copiedAllocator->m_currentPayloadEnd), scratchGPR);
                jit.addPtr(CCallHelpers::TrustedImm32(sizeof(JSValue)), scratchGPR);
                // We have scratchGPR = new storage, scratchGPR3 = old storage,
                // scratchGPR2 = available
                for (size_t offset = 0; offset < oldSize; offset += sizeof(void*)) {
                    jit.loadPtr(
                        CCallHelpers::Address(
                            scratchGPR3,
                            -static_cast<ptrdiff_t>(
                                offset + sizeof(JSValue) + sizeof(void*))),
                        scratchGPR2);
                    jit.storePtr(
                        scratchGPR2,
                        CCallHelpers::Address(
                            scratchGPR,
                            -static_cast<ptrdiff_t>(offset + sizeof(JSValue) + sizeof(void*))));
                }
            }

            jit.storePtr(scratchGPR, CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()));
            scratchGPRHasStorage = true;
        }

        uint32_t structureBits = bitwise_cast<uint32_t>(newStructure()->id());
        jit.store32(
            CCallHelpers::TrustedImm32(structureBits),
            CCallHelpers::Address(baseGPR, JSCell::structureIDOffset()));

        if (isInlineOffset(m_offset)) {
            jit.storeValue(
                valueRegs,
                CCallHelpers::Address(
                    baseGPR,
                    JSObject::offsetOfInlineStorage() +
                    offsetInInlineStorage(m_offset) * sizeof(JSValue)));
        } else {
            if (!scratchGPRHasStorage)
                jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
            jit.storeValue(
                valueRegs,
                CCallHelpers::Address(scratchGPR, offsetInButterfly(m_offset) * sizeof(JSValue)));
        }

        ScratchBuffer* scratchBuffer = nullptr;
        if (newStructure()->outOfLineCapacity() != structure()->outOfLineCapacity())
            scratchBuffer = vm.scratchBufferForSize(allocator.desiredScratchBufferSizeForCall());

        if (newStructure()->outOfLineCapacity() != structure()->outOfLineCapacity()) {
            CCallHelpers::Call callFlushWriteBarrierBuffer;
            CCallHelpers::Jump ownerIsRememberedOrInEden = jit.jumpIfIsRememberedOrInEden(baseGPR);
            WriteBarrierBuffer& writeBarrierBuffer = jit.vm()->heap.writeBarrierBuffer();
            jit.load32(writeBarrierBuffer.currentIndexAddress(), scratchGPR2);
            CCallHelpers::Jump needToFlush =
                jit.branch32(
                    CCallHelpers::AboveOrEqual, scratchGPR2,
                    CCallHelpers::TrustedImm32(writeBarrierBuffer.capacity()));

            jit.add32(CCallHelpers::TrustedImm32(1), scratchGPR2);
            jit.store32(scratchGPR2, writeBarrierBuffer.currentIndexAddress());

            jit.move(CCallHelpers::TrustedImmPtr(writeBarrierBuffer.buffer()), scratchGPR);
            // We use an offset of -sizeof(void*) because we already added 1 to scratchGPR2.
            jit.storePtr(
                baseGPR,
                CCallHelpers::BaseIndex(
                    scratchGPR, scratchGPR2, CCallHelpers::ScalePtr,
                    static_cast<int32_t>(-sizeof(void*))));

            CCallHelpers::Jump doneWithBarrier = jit.jump();
            needToFlush.link(&jit);

            // FIXME: We should restoreReusedRegistersByPopping() before this. Then, we wouldn't need
            // padding in preserveReusedRegistersByPushing(). Or, maybe it would be even better if the
            // barrier slow path was just the normal slow path, below.
            // https://bugs.webkit.org/show_bug.cgi?id=149030
            allocator.preserveUsedRegistersToScratchBufferForCall(jit, scratchBuffer, scratchGPR2);
            jit.setupArgumentsWithExecState(baseGPR);
            callFlushWriteBarrierBuffer = jit.call();
            allocator.restoreUsedRegistersFromScratchBufferForCall(
                jit, scratchBuffer, scratchGPR2);

            doneWithBarrier.link(&jit);
            ownerIsRememberedOrInEden.link(&jit);

            state.callbacks.append(
                [=] (LinkBuffer& linkBuffer) {
                    linkBuffer.link(callFlushWriteBarrierBuffer, operationFlushWriteBarrierBuffer);
                });
        }
        
        allocator.restoreReusedRegistersByPopping(jit, numberOfPaddingBytes);
        state.succeed();

        if (newStructure()->outOfLineCapacity() != structure()->outOfLineCapacity()) {
            slowPath.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, numberOfPaddingBytes);
            allocator.preserveUsedRegistersToScratchBufferForCall(jit, scratchBuffer, scratchGPR);
#if USE(JSVALUE64)
            jit.setupArgumentsWithExecState(
                baseGPR,
                CCallHelpers::TrustedImmPtr(newStructure()),
                CCallHelpers::TrustedImm32(m_offset),
                valueRegs.gpr());
#else
            jit.setupArgumentsWithExecState(
                baseGPR,
                CCallHelpers::TrustedImmPtr(newStructure()),
                CCallHelpers::TrustedImm32(m_offset),
                valueRegs.payloadGPR(), valueRegs.tagGPR());
#endif
            CCallHelpers::Call operationCall = jit.call();
            allocator.restoreUsedRegistersFromScratchBufferForCall(jit, scratchBuffer, scratchGPR);
            state.succeed();

            state.callbacks.append(
                [=] (LinkBuffer& linkBuffer) {
                    linkBuffer.link(operationCall, operationReallocateStorageAndFinishPut);
                });
        }
        return;
    }

    case ArrayLength: {
        jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
        jit.load32(CCallHelpers::Address(scratchGPR, ArrayStorage::lengthOffset()), scratchGPR);
        state.failAndIgnore.append(
            jit.branch32(CCallHelpers::LessThan, scratchGPR, CCallHelpers::TrustedImm32(0)));
        jit.boxInt32(scratchGPR, valueRegs);
        state.succeed();
        return;
    }

    case StringLength: {
        jit.load32(CCallHelpers::Address(baseGPR, JSString::offsetOfLength()), valueRegs.payloadGPR());
        jit.boxInt32(valueRegs.payloadGPR(), valueRegs);
        state.succeed();
        return;
    } }
    
    RELEASE_ASSERT_NOT_REACHED();
}

PolymorphicAccess::PolymorphicAccess() { }
PolymorphicAccess::~PolymorphicAccess() { }

MacroAssemblerCodePtr PolymorphicAccess::regenerateWithCases(
    VM& vm, CodeBlock* codeBlock, StructureStubInfo& stubInfo, const Identifier& ident,
    Vector<std::unique_ptr<AccessCase>> originalCasesToAdd)
{
    // This method will add the originalCasesToAdd to the list one at a time while preserving the
    // invariants:
    // - If a newly added case canReplace() any existing case, then the existing case is removed before
    //   the new case is added. Removal doesn't change order of the list. Any number of existing cases
    //   can be removed via the canReplace() rule.
    // - Cases in the list always appear in ascending order of time of addition. Therefore, if you
    //   cascade through the cases in reverse order, you will get the most recent cases first.
    // - If this method fails (returns null, doesn't add the cases), then both the previous case list
    //   and the previous stub are kept intact and the new cases are destroyed. It's OK to attempt to
    //   add more things after failure.
    
    // First, verify that we can generate code for all of the new cases while eliminating any of the
    // new cases that replace each other.
    Vector<std::unique_ptr<AccessCase>> casesToAdd;
    for (unsigned i = 0; i < originalCasesToAdd.size(); ++i) {
        std::unique_ptr<AccessCase> myCase = WTF::move(originalCasesToAdd[i]);

        // Add it only if it is not replaced by the subsequent cases in the list.
        bool found = false;
        for (unsigned j = i + 1; j < originalCasesToAdd.size(); ++j) {
            if (originalCasesToAdd[j]->canReplace(*myCase)) {
                found = true;
                break;
            }
        }

        if (found)
            continue;
        
        if (myCase->doesCalls() && stubInfo.patch.spillMode == NeedToSpill)
            return MacroAssemblerCodePtr();

        casesToAdd.append(WTF::move(myCase));
    }

    if (verbose)
        dataLog("casesToAdd: ", listDump(casesToAdd), "\n");

    // If there aren't any cases to add, then fail on the grounds that there's no point to generating a
    // new stub that will be identical to the old one. Returning null should tell the caller to just
    // keep doing what they were doing before.
    if (casesToAdd.isEmpty())
        return MacroAssemblerCodePtr();

    // Now construct the list of cases as they should appear if we are successful. This means putting
    // all of the previous cases in this list in order but excluding those that can be replaced, and
    // then adding the new cases.
    ListType newCases;
    for (auto& oldCase : m_list) {
        // Ignore old cases that cannot possibly succeed anymore.
        if (!oldCase->couldStillSucceed())
            continue;

        // Figure out if this is replaced by any new cases.
        bool found = false;
        for (auto& caseToAdd : casesToAdd) {
            if (caseToAdd->canReplace(*oldCase)) {
                found = true;
                break;
            }
        }
        if (found)
            continue;
        
        newCases.append(oldCase->clone());
    }
    for (auto& caseToAdd : casesToAdd)
        newCases.append(WTF::move(caseToAdd));

    if (verbose)
        dataLog("newCases: ", listDump(newCases), "\n");

    if (newCases.size() > Options::maxAccessVariantListSize()) {
        if (verbose)
            dataLog("Too many cases.\n");
        return MacroAssemblerCodePtr();
    }

    MacroAssemblerCodePtr result = regenerate(vm, codeBlock, stubInfo, ident, newCases);
    if (!result)
        return MacroAssemblerCodePtr();

    m_list = WTF::move(newCases);
    return result;
}

MacroAssemblerCodePtr PolymorphicAccess::regenerateWithCase(
    VM& vm, CodeBlock* codeBlock, StructureStubInfo& stubInfo, const Identifier& ident,
    std::unique_ptr<AccessCase> newAccess)
{
    Vector<std::unique_ptr<AccessCase>> newAccesses;
    newAccesses.append(WTF::move(newAccess));
    return regenerateWithCases(vm, codeBlock, stubInfo, ident, WTF::move(newAccesses));
}

bool PolymorphicAccess::visitWeak(VM& vm) const
{
    for (unsigned i = 0; i < size(); ++i) {
        if (!at(i).visitWeak(vm))
            return false;
    }
    if (Vector<WriteBarrier<JSCell>>* weakReferences = m_weakReferences.get()) {
        for (WriteBarrier<JSCell>& weakReference : *weakReferences) {
            if (!Heap::isMarked(weakReference.get()))
                return false;
        }
    }
    return true;
}

void PolymorphicAccess::dump(PrintStream& out) const
{
    out.print(RawPointer(this), ":[");
    CommaPrinter comma;
    for (auto& entry : m_list)
        out.print(comma, *entry);
    out.print("]");
}

MacroAssemblerCodePtr PolymorphicAccess::regenerate(
    VM& vm, CodeBlock* codeBlock, StructureStubInfo& stubInfo, const Identifier& ident,
    PolymorphicAccess::ListType& cases)
{
    if (verbose)
        dataLog("Generating code for cases: ", listDump(cases), "\n");
    
    AccessGenerationState state;

    state.access = this;
    state.stubInfo = &stubInfo;
    state.ident = &ident;
    
    state.baseGPR = static_cast<GPRReg>(stubInfo.patch.baseGPR);
    state.valueRegs = JSValueRegs(
#if USE(JSVALUE32_64)
        static_cast<GPRReg>(stubInfo.patch.valueTagGPR),
#endif
        static_cast<GPRReg>(stubInfo.patch.valueGPR));

    ScratchRegisterAllocator allocator(stubInfo.patch.usedRegisters);
    state.allocator = &allocator;
    allocator.lock(state.baseGPR);
    allocator.lock(state.valueRegs);

    state.scratchGPR = allocator.allocateScratchGPR();
    
    CCallHelpers jit(&vm, codeBlock);
    state.jit = &jit;

    state.numberOfPaddingBytes = allocator.preserveReusedRegistersByPushing(jit);

    bool allGuardedByStructureCheck = true;
    for (auto& entry : cases)
        allGuardedByStructureCheck &= entry->guardedByStructureCheck();

    if (cases.isEmpty()) {
        // This is super unlikely, but we make it legal anyway.
        state.failAndRepatch.append(jit.jump());
    } else if (!allGuardedByStructureCheck || cases.size() == 1) {
        // If there are any proxies in the list, we cannot just use a binary switch over the structure.
        // We need to resort to a cascade. A cascade also happens to be optimal if we only have just
        // one case.
        CCallHelpers::JumpList fallThrough;

        // Cascade through the list, preferring newer entries.
        for (unsigned i = cases.size(); i--;) {
            fallThrough.link(&jit);
            cases[i]->generateWithGuard(state, fallThrough);
        }
        state.failAndRepatch.append(fallThrough);
    } else {
        jit.load32(
            CCallHelpers::Address(state.baseGPR, JSCell::structureIDOffset()),
            state.scratchGPR);
        
        Vector<int64_t> caseValues(cases.size());
        for (unsigned i = 0; i < cases.size(); ++i)
            caseValues[i] = bitwise_cast<int32_t>(cases[i]->structure()->id());
        
        BinarySwitch binarySwitch(state.scratchGPR, caseValues, BinarySwitch::Int32);
        while (binarySwitch.advance(jit))
            cases[binarySwitch.caseIndex()]->generate(state);
        state.failAndRepatch.append(binarySwitch.fallThrough());
    }

    state.failAndIgnore.link(&jit);

    // Make sure that the inline cache optimization code knows that we are taking slow path because
    // of something that isn't patchable. "seen" being false means that we bypass patching. This is
    // pretty gross but it means that we don't need to have two slow path entrypoints - one for
    // patching and one for normal slow stuff.
    jit.store8(CCallHelpers::TrustedImm32(false), &stubInfo.seen);

    CCallHelpers::JumpList failure;
    if (allocator.didReuseRegisters()) {
        state.failAndRepatch.link(&jit);
        state.restoreScratch();
    } else
        failure = state.failAndRepatch;
    failure.append(jit.jump());

    LinkBuffer linkBuffer(vm, jit, codeBlock, JITCompilationCanFail);
    if (linkBuffer.didFailToAllocate()) {
        if (verbose)
            dataLog("Did fail to allocate.\n");
        return MacroAssemblerCodePtr();
    }

    CodeLocationLabel successLabel =
        stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToDone);
        
    linkBuffer.link(state.success, successLabel);

    linkBuffer.link(
        failure,
        stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToSlowCase));
    
    for (auto callback : state.callbacks)
        callback(linkBuffer);
    
    MacroAssemblerCodeRef code = FINALIZE_CODE_FOR(
        codeBlock, linkBuffer,
        ("%s", toCString("Access stub for ", *codeBlock, " ", stubInfo.codeOrigin, " with return point ", successLabel, ": ", listDump(cases)).data()));

    bool doesCalls = false;
    for (auto& entry : cases)
        doesCalls |= entry->doesCalls();
    
    m_stubRoutine = createJITStubRoutine(code, vm, codeBlock->ownerExecutable(), doesCalls);
    m_watchpoints = WTF::move(state.watchpoints);
    if (!state.weakReferences.isEmpty())
        m_weakReferences = std::make_unique<Vector<WriteBarrier<JSCell>>>(WTF::move(state.weakReferences));
    if (verbose)
        dataLog("Returning: ", code.code(), "\n");
    return code.code();
}

} // namespace JSC

namespace WTF {

using namespace JSC;

void printInternal(PrintStream& out, AccessCase::AccessType type)
{
    switch (type) {
    case AccessCase::Load:
        out.print("Load");
        return;
    case AccessCase::Transition:
        out.print("Transition");
        return;
    case AccessCase::Replace:
        out.print("Replace");
        return;
    case AccessCase::Miss:
        out.print("Miss");
        return;
    case AccessCase::Getter:
        out.print("Getter");
        return;
    case AccessCase::Setter:
        out.print("Setter");
        return;
    case AccessCase::CustomGetter:
        out.print("CustomGetter");
        return;
    case AccessCase::CustomSetter:
        out.print("CustomSetter");
        return;
    case AccessCase::InHit:
        out.print("InHit");
        return;
    case AccessCase::InMiss:
        out.print("InMiss");
        return;
    case AccessCase::ArrayLength:
        out.print("ArrayLength");
        return;
    case AccessCase::StringLength:
        out.print("StringLength");
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WTF

#endif // ENABLE(JIT)


