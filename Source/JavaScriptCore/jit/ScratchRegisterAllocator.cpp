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
#include "ScratchRegisterAllocator.h"

#if ENABLE(JIT)

#include "JSCInlines.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "VM.h"

namespace JSC {

ScratchRegisterAllocator::ScratchRegisterAllocator(const RegisterSet& usedRegisters)
    : m_usedRegisters(usedRegisters)
    , m_numberOfReusedRegisters(0)
{
}

ScratchRegisterAllocator::~ScratchRegisterAllocator() { }

void ScratchRegisterAllocator::lock(GPRReg reg)
{
    if (reg == InvalidGPRReg)
        return;
    unsigned index = GPRInfo::toIndex(reg);
    if (index == GPRInfo::InvalidIndex)
        return;
    m_lockedRegisters.setGPRByIndex(index);
}

void ScratchRegisterAllocator::lock(FPRReg reg)
{
    if (reg == InvalidFPRReg)
        return;
    unsigned index = FPRInfo::toIndex(reg);
    if (index == FPRInfo::InvalidIndex)
        return;
    m_lockedRegisters.setFPRByIndex(index);
}

void ScratchRegisterAllocator::lock(JSValueRegs regs)
{
    lock(regs.tagGPR());
    lock(regs.payloadGPR());
}

template<typename BankInfo>
typename BankInfo::RegisterType ScratchRegisterAllocator::allocateScratch()
{
    // First try to allocate a register that is totally free.
    for (unsigned i = 0; i < BankInfo::numberOfRegisters; ++i) {
        typename BankInfo::RegisterType reg = BankInfo::toRegister(i);
        if (!m_lockedRegisters.get(reg)
            && !m_usedRegisters.get(reg)
            && !m_scratchRegisters.get(reg)) {
            m_scratchRegisters.set(reg);
            return reg;
        }
    }
    
    // Since that failed, try to allocate a register that is not yet
    // locked or used for scratch.
    for (unsigned i = 0; i < BankInfo::numberOfRegisters; ++i) {
        typename BankInfo::RegisterType reg = BankInfo::toRegister(i);
        if (!m_lockedRegisters.get(reg) && !m_scratchRegisters.get(reg)) {
            m_scratchRegisters.set(reg);
            m_numberOfReusedRegisters++;
            return reg;
        }
    }
        
    // We failed.
    CRASH();
    // Make some silly compilers happy.
    return static_cast<typename BankInfo::RegisterType>(-1);
}

GPRReg ScratchRegisterAllocator::allocateScratchGPR() { return allocateScratch<GPRInfo>(); }
FPRReg ScratchRegisterAllocator::allocateScratchFPR() { return allocateScratch<FPRInfo>(); }

size_t ScratchRegisterAllocator::preserveReusedRegistersByPushing(MacroAssembler& jit)
{
    if (!didReuseRegisters())
        return 0;

    size_t numberOfBytesPushed = 0;

    for (unsigned i = 0; i < FPRInfo::numberOfRegisters; ++i) {
        FPRReg reg = FPRInfo::toRegister(i);
        if (m_scratchRegisters.getFPRByIndex(i) && m_usedRegisters.get(reg)) {
            jit.pushToSave(reg);
            numberOfBytesPushed += sizeof(double);
        }
    }
    for (unsigned i = 0; i < GPRInfo::numberOfRegisters; ++i) {
        GPRReg reg = GPRInfo::toRegister(i);
        if (m_scratchRegisters.getGPRByIndex(i) && m_usedRegisters.get(reg)) {
            jit.pushToSave(reg);
            numberOfBytesPushed += sizeof(uintptr_t);
        }
    }

    size_t totalStackAdjustmentBytes = numberOfBytesPushed + maxFrameExtentForSlowPathCall;
    totalStackAdjustmentBytes = WTF::roundUpToMultipleOf(stackAlignmentBytes(), totalStackAdjustmentBytes);

    // FIXME: We shouldn't have to do this.
    // https://bugs.webkit.org/show_bug.cgi?id=149030
    size_t numberOfPaddingBytes = totalStackAdjustmentBytes - numberOfBytesPushed;
    jit.subPtr(MacroAssembler::TrustedImm32(numberOfPaddingBytes), MacroAssembler::stackPointerRegister);

    return numberOfPaddingBytes;
}

void ScratchRegisterAllocator::restoreReusedRegistersByPopping(MacroAssembler& jit, size_t numberOfPaddingBytes)
{
    if (!didReuseRegisters())
        return;

    jit.addPtr(MacroAssembler::TrustedImm32(numberOfPaddingBytes), MacroAssembler::stackPointerRegister);

    for (unsigned i = GPRInfo::numberOfRegisters; i--;) {
        GPRReg reg = GPRInfo::toRegister(i);
        if (m_scratchRegisters.getGPRByIndex(i) && m_usedRegisters.get(reg))
            jit.popToRestore(reg);
    }
    for (unsigned i = FPRInfo::numberOfRegisters; i--;) {
        FPRReg reg = FPRInfo::toRegister(i);
        if (m_scratchRegisters.getFPRByIndex(i) && m_usedRegisters.get(reg))
            jit.popToRestore(reg);
    }
}

RegisterSet ScratchRegisterAllocator::usedRegistersForCall() const
{
    RegisterSet result = m_usedRegisters;
    result.exclude(RegisterSet::calleeSaveRegisters());
    result.exclude(RegisterSet::stackRegisters());
    result.exclude(RegisterSet::reservedHardwareRegisters());
    return result;
}

unsigned ScratchRegisterAllocator::desiredScratchBufferSizeForCall() const
{
    return usedRegistersForCall().numberOfSetRegisters() * sizeof(JSValue);
}

void ScratchRegisterAllocator::preserveUsedRegistersToScratchBufferForCall(MacroAssembler& jit, ScratchBuffer* scratchBuffer, GPRReg scratchGPR)
{
    RegisterSet usedRegisters = usedRegistersForCall();
    if (!usedRegisters.numberOfSetRegisters())
        return;
    
    unsigned count = 0;
    for (GPRReg reg = MacroAssembler::firstRegister(); reg <= MacroAssembler::lastRegister(); reg = MacroAssembler::nextRegister(reg)) {
        if (usedRegisters.get(reg))
            jit.storePtr(reg, static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer()) + (count++));
        if (GPRInfo::toIndex(reg) != GPRInfo::InvalidIndex
            && scratchGPR == InvalidGPRReg
            && !m_lockedRegisters.get(reg) && !m_scratchRegisters.get(reg))
            scratchGPR = reg;
    }
    RELEASE_ASSERT(scratchGPR != InvalidGPRReg);
    for (FPRReg reg = MacroAssembler::firstFPRegister(); reg <= MacroAssembler::lastFPRegister(); reg = MacroAssembler::nextFPRegister(reg)) {
        if (usedRegisters.get(reg)) {
            jit.move(MacroAssembler::TrustedImmPtr(static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer()) + (count++)), scratchGPR);
            jit.storeDouble(reg, scratchGPR);
        }
    }
    RELEASE_ASSERT(count * sizeof(JSValue) == desiredScratchBufferSizeForCall());
    
    jit.move(MacroAssembler::TrustedImmPtr(scratchBuffer->activeLengthPtr()), scratchGPR);
    jit.storePtr(MacroAssembler::TrustedImmPtr(static_cast<size_t>(count * sizeof(JSValue))), scratchGPR);
}

void ScratchRegisterAllocator::restoreUsedRegistersFromScratchBufferForCall(MacroAssembler& jit, ScratchBuffer* scratchBuffer, GPRReg scratchGPR)
{
    RegisterSet usedRegisters = usedRegistersForCall();
    if (!usedRegisters.numberOfSetRegisters())
        return;
    
    if (scratchGPR == InvalidGPRReg) {
        // Find a scratch register.
        for (unsigned i = GPRInfo::numberOfRegisters; i--;) {
            if (m_lockedRegisters.getGPRByIndex(i) || m_scratchRegisters.getGPRByIndex(i))
                continue;
            scratchGPR = GPRInfo::toRegister(i);
            break;
        }
    }
    RELEASE_ASSERT(scratchGPR != InvalidGPRReg);
    
    jit.move(MacroAssembler::TrustedImmPtr(scratchBuffer->activeLengthPtr()), scratchGPR);
    jit.storePtr(MacroAssembler::TrustedImmPtr(0), scratchGPR);

    // Restore double registers first.
    unsigned count = usedRegisters.numberOfSetGPRs();
    for (FPRReg reg = MacroAssembler::firstFPRegister(); reg <= MacroAssembler::lastFPRegister(); reg = MacroAssembler::nextFPRegister(reg)) {
        if (usedRegisters.get(reg)) {
            jit.move(MacroAssembler::TrustedImmPtr(static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer()) + (count++)), scratchGPR);
            jit.loadDouble(scratchGPR, reg);
        }
    }
        
    count = 0;
    for (GPRReg reg = MacroAssembler::firstRegister(); reg <= MacroAssembler::lastRegister(); reg = MacroAssembler::nextRegister(reg)) {
        if (usedRegisters.get(reg))
            jit.loadPtr(static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer()) + (count++), reg);
    }
}

} // namespace JSC

#endif // ENABLE(JIT)
