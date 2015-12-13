/*
 * Copyright (C) 2009, 2010, 2012-2015 Apple Inc. All rights reserved.
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

#ifndef LinkBuffer_h
#define LinkBuffer_h

#if ENABLE(ASSEMBLER)

#define DUMP_LINK_STATISTICS 0
#define DUMP_CODE 0

#define GLOBAL_THUNK_ID reinterpret_cast<void*>(static_cast<intptr_t>(-1))
#define REGEXP_CODE_ID reinterpret_cast<void*>(static_cast<intptr_t>(-2))
#define CSS_CODE_ID reinterpret_cast<void*>(static_cast<intptr_t>(-3))

#include "JITCompilationEffort.h"
#include "MacroAssembler.h"
#include <wtf/DataLog.h>
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>

namespace JSC {

class CodeBlock;
class VM;

// LinkBuffer:
//
// This class assists in linking code generated by the macro assembler, once code generation
// has been completed, and the code has been copied to is final location in memory.  At this
// time pointers to labels within the code may be resolved, and relative offsets to external
// addresses may be fixed.
//
// Specifically:
//   * Jump objects may be linked to external targets,
//   * The address of Jump objects may taken, such that it can later be relinked.
//   * The return address of a Call may be acquired.
//   * The address of a Label pointing into the code may be resolved.
//   * The value referenced by a DataLabel may be set.
//
class LinkBuffer {
    WTF_MAKE_NONCOPYABLE(LinkBuffer); WTF_MAKE_FAST_ALLOCATED;
    
    typedef MacroAssemblerCodeRef CodeRef;
    typedef MacroAssemblerCodePtr CodePtr;
    typedef MacroAssembler::Label Label;
    typedef MacroAssembler::Jump Jump;
    typedef MacroAssembler::PatchableJump PatchableJump;
    typedef MacroAssembler::JumpList JumpList;
    typedef MacroAssembler::Call Call;
    typedef MacroAssembler::DataLabelCompact DataLabelCompact;
    typedef MacroAssembler::DataLabel32 DataLabel32;
    typedef MacroAssembler::DataLabelPtr DataLabelPtr;
    typedef MacroAssembler::ConvertibleLoadLabel ConvertibleLoadLabel;
#if ENABLE(BRANCH_COMPACTION)
    typedef MacroAssembler::LinkRecord LinkRecord;
    typedef MacroAssembler::JumpLinkType JumpLinkType;
#endif

public:
    LinkBuffer(VM& vm, MacroAssembler& macroAssembler, void* ownerUID, JITCompilationEffort effort = JITCompilationMustSucceed)
        : m_size(0)
#if ENABLE(BRANCH_COMPACTION)
        , m_initialSize(0)
#endif
        , m_didAllocate(false)
        , m_code(0)
        , m_vm(&vm)
#ifndef NDEBUG
        , m_completed(false)
#endif
    {
        linkCode(macroAssembler, ownerUID, effort);
    }

    LinkBuffer(VM& vm, MacroAssembler& macroAssembler, void* code, size_t size)
        : m_size(size)
#if ENABLE(BRANCH_COMPACTION)
        , m_initialSize(0)
#endif
        , m_didAllocate(false)
        , m_code(code)
        , m_vm(&vm)
#ifndef NDEBUG
        , m_completed(false)
#endif
    {
        linkCode(macroAssembler, 0, JITCompilationCanFail);
    }

    ~LinkBuffer()
    {
    }
    
    bool didFailToAllocate() const
    {
        return !m_didAllocate;
    }

    bool isValid() const
    {
        return !didFailToAllocate();
    }
    
    // These methods are used to link or set values at code generation time.

    void link(Call call, FunctionPtr function)
    {
        ASSERT(call.isFlagSet(Call::Linkable));
        call.m_label = applyOffset(call.m_label);
        MacroAssembler::linkCall(code(), call, function);
    }
    
    void link(Call call, CodeLocationLabel label)
    {
        link(call, FunctionPtr(label.executableAddress()));
    }
    
    void link(Jump jump, CodeLocationLabel label)
    {
        jump.m_label = applyOffset(jump.m_label);
        MacroAssembler::linkJump(code(), jump, label);
    }

    void link(JumpList list, CodeLocationLabel label)
    {
        for (unsigned i = 0; i < list.m_jumps.size(); ++i)
            link(list.m_jumps[i], label);
    }

    void patch(DataLabelPtr label, void* value)
    {
        AssemblerLabel target = applyOffset(label.m_label);
        MacroAssembler::linkPointer(code(), target, value);
    }

    void patch(DataLabelPtr label, CodeLocationLabel value)
    {
        AssemblerLabel target = applyOffset(label.m_label);
        MacroAssembler::linkPointer(code(), target, value.executableAddress());
    }

    // These methods are used to obtain handles to allow the code to be relinked / repatched later.
    
    CodeLocationLabel entrypoint()
    {
        return CodeLocationLabel(code());
    }

    CodeLocationCall locationOf(Call call)
    {
        ASSERT(call.isFlagSet(Call::Linkable));
        ASSERT(!call.isFlagSet(Call::Near));
        return CodeLocationCall(MacroAssembler::getLinkerAddress(code(), applyOffset(call.m_label)));
    }

    CodeLocationNearCall locationOfNearCall(Call call)
    {
        ASSERT(call.isFlagSet(Call::Linkable));
        ASSERT(call.isFlagSet(Call::Near));
        return CodeLocationNearCall(MacroAssembler::getLinkerAddress(code(), applyOffset(call.m_label)),
            call.isFlagSet(Call::Tail) ? NearCallMode::Tail : NearCallMode::Regular);
    }

    CodeLocationLabel locationOf(PatchableJump jump)
    {
        return CodeLocationLabel(MacroAssembler::getLinkerAddress(code(), applyOffset(jump.m_jump.m_label)));
    }

    CodeLocationLabel locationOf(Label label)
    {
        return CodeLocationLabel(MacroAssembler::getLinkerAddress(code(), applyOffset(label.m_label)));
    }

    CodeLocationDataLabelPtr locationOf(DataLabelPtr label)
    {
        return CodeLocationDataLabelPtr(MacroAssembler::getLinkerAddress(code(), applyOffset(label.m_label)));
    }

    CodeLocationDataLabel32 locationOf(DataLabel32 label)
    {
        return CodeLocationDataLabel32(MacroAssembler::getLinkerAddress(code(), applyOffset(label.m_label)));
    }
    
    CodeLocationDataLabelCompact locationOf(DataLabelCompact label)
    {
        return CodeLocationDataLabelCompact(MacroAssembler::getLinkerAddress(code(), applyOffset(label.m_label)));
    }

    CodeLocationConvertibleLoad locationOf(ConvertibleLoadLabel label)
    {
        return CodeLocationConvertibleLoad(MacroAssembler::getLinkerAddress(code(), applyOffset(label.m_label)));
    }

    // This method obtains the return address of the call, given as an offset from
    // the start of the code.
    unsigned returnAddressOffset(Call call)
    {
        call.m_label = applyOffset(call.m_label);
        return MacroAssembler::getLinkerCallReturnOffset(call);
    }

    uint32_t offsetOf(Label label)
    {
        return applyOffset(label.m_label).m_offset;
    }

    unsigned offsetOf(PatchableJump jump)
    {
        return applyOffset(jump.m_jump.m_label).m_offset;
    }

    // Upon completion of all patching 'FINALIZE_CODE()' should be called once to
    // complete generation of the code. Alternatively, call
    // finalizeCodeWithoutDisassembly() directly if you have your own way of
    // displaying disassembly.
    
    JS_EXPORT_PRIVATE CodeRef finalizeCodeWithoutDisassembly();
    JS_EXPORT_PRIVATE CodeRef finalizeCodeWithDisassembly(const char* format, ...) WTF_ATTRIBUTE_PRINTF(2, 3);

    CodePtr trampolineAt(Label label)
    {
        return CodePtr(MacroAssembler::AssemblerType_T::getRelocatedAddress(code(), applyOffset(label.m_label)));
    }

    void* debugAddress()
    {
        return m_code;
    }

    // FIXME: this does not account for the AssemblerData size!
    size_t size()
    {
        return m_size;
    }
    
    bool wasAlreadyDisassembled() const { return m_alreadyDisassembled; }
    void didAlreadyDisassemble() { m_alreadyDisassembled = true; }

private:
#if ENABLE(BRANCH_COMPACTION)
    int executableOffsetFor(int location)
    {
        if (!location)
            return 0;
        return bitwise_cast<int32_t*>(m_assemblerStorage.buffer())[location / sizeof(int32_t) - 1];
    }
#endif
    
    template <typename T> T applyOffset(T src)
    {
#if ENABLE(BRANCH_COMPACTION)
        src.m_offset -= executableOffsetFor(src.m_offset);
#endif
        return src;
    }
    
    // Keep this private! - the underlying code should only be obtained externally via finalizeCode().
    void* code()
    {
        return m_code;
    }
    
    void allocate(size_t initialSize, void* ownerUID, JITCompilationEffort);
    void shrink(size_t newSize);

    JS_EXPORT_PRIVATE void linkCode(MacroAssembler&, void* ownerUID, JITCompilationEffort);
#if ENABLE(BRANCH_COMPACTION)
    template <typename InstructionType>
    void copyCompactAndLinkCode(MacroAssembler&, void* ownerUID, JITCompilationEffort);
#endif

    void performFinalization();

#if DUMP_LINK_STATISTICS
    static void dumpLinkStatistics(void* code, size_t initialSize, size_t finalSize);
#endif
    
#if DUMP_CODE
    static void dumpCode(void* code, size_t);
#endif
    
    RefPtr<ExecutableMemoryHandle> m_executableMemory;
    size_t m_size;
#if ENABLE(BRANCH_COMPACTION)
    size_t m_initialSize;
    AssemblerData m_assemblerStorage;
#endif
    bool m_didAllocate;
    void* m_code;
    VM* m_vm;
#ifndef NDEBUG
    bool m_completed;
#endif
    bool m_alreadyDisassembled { false };
};

#define FINALIZE_CODE_IF(condition, linkBufferReference, dataLogFArgumentsForHeading)  \
    (UNLIKELY((condition))                                              \
     ? ((linkBufferReference).finalizeCodeWithDisassembly dataLogFArgumentsForHeading) \
     : (linkBufferReference).finalizeCodeWithoutDisassembly())

bool shouldShowDisassemblyFor(CodeBlock*);

#define FINALIZE_CODE_FOR(codeBlock, linkBufferReference, dataLogFArgumentsForHeading)  \
    FINALIZE_CODE_IF(shouldShowDisassemblyFor(codeBlock) || Options::asyncDisassembly(), linkBufferReference, dataLogFArgumentsForHeading)

// Use this to finalize code, like so:
//
// CodeRef code = FINALIZE_CODE(linkBuffer, ("my super thingy number %d", number));
//
// Which, in disassembly mode, will print:
//
// Generated JIT code for my super thingy number 42:
//     Code at [0x123456, 0x234567]:
//         0x123456: mov $0, 0
//         0x12345a: ret
//
// ... and so on.
//
// Note that the dataLogFArgumentsForHeading are only evaluated when showDisassembly
// is true, so you can hide expensive disassembly-only computations inside there.

#define FINALIZE_CODE(linkBufferReference, dataLogFArgumentsForHeading)  \
    FINALIZE_CODE_IF(JSC::Options::asyncDisassembly() || JSC::Options::showDisassembly(), linkBufferReference, dataLogFArgumentsForHeading)

#define FINALIZE_DFG_CODE(linkBufferReference, dataLogFArgumentsForHeading)  \
    FINALIZE_CODE_IF(JSC::Options::asyncDisassembly() || JSC::Options::showDisassembly() || Options::showDFGDisassembly(), linkBufferReference, dataLogFArgumentsForHeading)

} // namespace JSC

#endif // ENABLE(ASSEMBLER)

#endif // LinkBuffer_h
