/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "JSDollarVMPrototype.h"

#include "Heap.h"
#include "HeapIterationScope.h"
#include "JSCInlines.h"
#include "JSFunction.h"
#include "StackVisitor.h"
#include <wtf/DataLog.h>

namespace JSC {

const ClassInfo JSDollarVMPrototype::s_info = { "DollarVMPrototype", &Base::s_info, 0, CREATE_METHOD_TABLE(JSDollarVMPrototype) };
    
    
bool JSDollarVMPrototype::currentThreadOwnsJSLock(ExecState* exec)
{
    return exec->vm().apiLock().currentThreadIsHoldingLock();
}

static bool ensureCurrentThreadOwnsJSLock(ExecState* exec)
{
    if (JSDollarVMPrototype::currentThreadOwnsJSLock(exec))
        return true;
    dataLog("ERROR: current thread does not own the JSLock\n");
    return false;
}

void JSDollarVMPrototype::addFunction(VM& vm, JSGlobalObject* globalObject, const char* name, NativeFunction function, unsigned arguments)
{
    Identifier identifier = Identifier::fromString(&vm, name);
    putDirect(vm, identifier, JSFunction::create(vm, globalObject, arguments, identifier.string(), function));
}

static EncodedJSValue JSC_HOST_CALL functionCrash(ExecState*)
{
    CRASH();
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL functionDFGTrue(ExecState*)
{
    return JSValue::encode(jsBoolean(false));
}

class CallerFrameJITTypeFunctor {
public:
    CallerFrameJITTypeFunctor()
        : m_currentFrame(0)
        , m_jitType(JITCode::None)
    {
    }

    StackVisitor::Status operator()(StackVisitor& visitor)
    {
        if (m_currentFrame++ > 1) {
            m_jitType = visitor->codeBlock()->jitType();
            return StackVisitor::Done;
        }
        return StackVisitor::Continue;
    }
    
    JITCode::JITType jitType() { return m_jitType; }

private:
    unsigned m_currentFrame;
    JITCode::JITType m_jitType;
};

static EncodedJSValue JSC_HOST_CALL functionLLintTrue(ExecState* exec)
{
    if (!exec)
        return JSValue::encode(jsUndefined());
    CallerFrameJITTypeFunctor functor;
    exec->iterate(functor);
    return JSValue::encode(jsBoolean(functor.jitType() == JITCode::InterpreterThunk));
}

static EncodedJSValue JSC_HOST_CALL functionJITTrue(ExecState* exec)
{
    if (!exec)
        return JSValue::encode(jsUndefined());
    CallerFrameJITTypeFunctor functor;
    exec->iterate(functor);
    return JSValue::encode(jsBoolean(functor.jitType() == JITCode::BaselineJIT));
}

void JSDollarVMPrototype::gc(ExecState* exec)
{
    if (!ensureCurrentThreadOwnsJSLock(exec))
        return;
    exec->heap()->collectAllGarbage();
}
    
static EncodedJSValue JSC_HOST_CALL functionGC(ExecState* exec)
{
    JSDollarVMPrototype::gc(exec);
    return JSValue::encode(jsUndefined());
}

void JSDollarVMPrototype::edenGC(ExecState* exec)
{
    if (!ensureCurrentThreadOwnsJSLock(exec))
        return;
    exec->heap()->collectAndSweep(EdenCollection);
}

static EncodedJSValue JSC_HOST_CALL functionEdenGC(ExecState* exec)
{
    JSDollarVMPrototype::edenGC(exec);
    return JSValue::encode(jsUndefined());
}

bool JSDollarVMPrototype::isInHeap(Heap* heap, void* ptr)
{
    return isInObjectSpace(heap, ptr) || isInStorageSpace(heap, ptr);
}

bool JSDollarVMPrototype::isInObjectSpace(Heap* heap, void* ptr)
{
    MarkedBlock* candidate = MarkedBlock::blockFor(ptr);
    return heap->objectSpace().blocks().set().contains(candidate);
}

bool JSDollarVMPrototype::isInStorageSpace(Heap* heap, void* ptr)
{
    CopiedBlock* candidate = CopiedSpace::blockFor(ptr);
    return heap->storageSpace().contains(candidate);
}

struct CellAddressCheckFunctor : MarkedBlock::CountFunctor {
    CellAddressCheckFunctor(JSCell* candidate)
        : candidate(candidate)
    {
    }

    IterationStatus operator()(JSCell* cell)
    {
        if (cell == candidate) {
            found = true;
            return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    }

    JSCell* candidate;
    bool found { false };
};

bool JSDollarVMPrototype::isValidCell(Heap* heap, JSCell* candidate)
{
    HeapIterationScope iterationScope(*heap);
    CellAddressCheckFunctor functor(candidate);
    heap->objectSpace().forEachLiveCell(iterationScope, functor);
    return functor.found;
}

bool JSDollarVMPrototype::isValidCodeBlock(ExecState* exec, CodeBlock* candidate)
{
    if (!ensureCurrentThreadOwnsJSLock(exec))
        return false;
    
    struct CodeBlockValidationFunctor {
        CodeBlockValidationFunctor(CodeBlock* candidate)
            : candidate(candidate)
        {
        }
        
        bool operator()(CodeBlock* codeBlock)
        {
            if (codeBlock == candidate)
                found = true;
            return found;
        }
        
        CodeBlock* candidate;
        bool found { false };
    };
    
    VM& vm = exec->vm();
    CodeBlockValidationFunctor functor(candidate);
    vm.heap.forEachCodeBlock(functor);
    return functor.found;
}

CodeBlock* JSDollarVMPrototype::codeBlockForFrame(CallFrame* topCallFrame, unsigned frameNumber)
{
    if (!ensureCurrentThreadOwnsJSLock(topCallFrame))
        return nullptr;
    
    if (!topCallFrame)
        return nullptr;
    
    struct FetchCodeBlockFunctor {
    public:
        FetchCodeBlockFunctor(unsigned targetFrameNumber)
            : targetFrame(targetFrameNumber)
        {
        }
        
        StackVisitor::Status operator()(StackVisitor& visitor)
        {
            currentFrame++;
            if (currentFrame == targetFrame) {
                codeBlock = visitor->codeBlock();
                return StackVisitor::Done;
            }
            return StackVisitor::Continue;
        }
        
        unsigned targetFrame;
        unsigned currentFrame { 0 };
        CodeBlock* codeBlock { nullptr };
    };
    
    FetchCodeBlockFunctor functor(frameNumber);
    topCallFrame->iterate(functor);
    return functor.codeBlock;
}

static EncodedJSValue JSC_HOST_CALL functionCodeBlockForFrame(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return JSValue::encode(jsUndefined());

    JSValue value = exec->uncheckedArgument(0);
    if (!value.isUInt32())
        return JSValue::encode(jsUndefined());

    // We need to inc the frame number because the caller would consider
    // its own frame as frame 0. Hence, we need discount the frame for this
    // function.
    unsigned frameNumber = value.asUInt32() + 1;
    CodeBlock* codeBlock = JSDollarVMPrototype::codeBlockForFrame(exec, frameNumber);
    return JSValue::encode(JSValue(bitwise_cast<double>(reinterpret_cast<uint64_t>(codeBlock))));
}

static CodeBlock* codeBlockFromArg(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return nullptr;

    JSValue value = exec->uncheckedArgument(0);
    if (!value.isDouble()) {
        dataLog("Invalid codeBlock: ", value, "\n");
        return nullptr;
    }

    CodeBlock* codeBlock = reinterpret_cast<CodeBlock*>(bitwise_cast<uint64_t>(value.asDouble()));
    if (JSDollarVMPrototype::isValidCodeBlock(exec, codeBlock))
        return codeBlock;

    dataLogF("Invalid codeBlock: %p ", codeBlock);
    dataLog(value, "\n");
    return nullptr;
    
}

static EncodedJSValue JSC_HOST_CALL functionPrintSourceFor(ExecState* exec)
{
    CodeBlock* codeBlock = codeBlockFromArg(exec);
    if (codeBlock)
        codeBlock->dumpSource();
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL functionPrintByteCodeFor(ExecState* exec)
{
    CodeBlock* codeBlock = codeBlockFromArg(exec);
    if (codeBlock)
        codeBlock->dumpBytecode();
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL functionPrint(ExecState* exec)
{
    for (unsigned i = 0; i < exec->argumentCount(); ++i) {
        if (i)
            dataLog(" ");
        dataLog(exec->uncheckedArgument(i).toString(exec)->value(exec));
    }
    return JSValue::encode(jsUndefined());
}

class PrintFrameFunctor {
public:
    enum Action {
        PrintOne,
        PrintAll
    };
    
    PrintFrameFunctor(Action action, unsigned framesToSkip)
        : m_action(action)
        , m_framesToSkip(framesToSkip)
    {
    }
    
    StackVisitor::Status operator()(StackVisitor& visitor)
    {
        m_currentFrame++;
        if (m_currentFrame > m_framesToSkip)
            visitor->print(2);
        
        if (m_action == PrintOne && m_currentFrame > m_framesToSkip)
            return StackVisitor::Done;
        return StackVisitor::Continue;
    }
    
private:
    Action m_action;
    unsigned m_framesToSkip;
    unsigned m_currentFrame { 0 };
};

static void printCallFrame(CallFrame* callFrame, unsigned framesToSkip)
{
    if (!ensureCurrentThreadOwnsJSLock(callFrame))
        return;
    PrintFrameFunctor functor(PrintFrameFunctor::PrintOne, framesToSkip);
    callFrame->iterate(functor);
}

void JSDollarVMPrototype::printCallFrame(CallFrame* callFrame)
{
    JSC::printCallFrame(callFrame, 0);
}

static void printStack(CallFrame* topCallFrame, unsigned framesToSkip)
{
    if (!ensureCurrentThreadOwnsJSLock(topCallFrame))
        return;
    if (!topCallFrame)
        return;
    PrintFrameFunctor functor(PrintFrameFunctor::PrintAll, framesToSkip);
    topCallFrame->iterate(functor);
}

void JSDollarVMPrototype::printStack(CallFrame* topCallFrame)
{
    JSC::printStack(topCallFrame, 0);
}

static EncodedJSValue JSC_HOST_CALL functionPrintCallFrame(ExecState* exec)
{
    // When the callers call this function, they are expecting to print their
    // own frame. So skip 1 for this frame.
    printCallFrame(exec, 1);
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL functionPrintStack(ExecState* exec)
{
    // When the callers call this function, they are expecting to print the
    // stack starting their own frame. So skip 1 for this frame.
    printStack(exec, 1);
    return JSValue::encode(jsUndefined());
}

void JSDollarVMPrototype::printValue(JSValue value)
{
    dataLog(value);
}

static EncodedJSValue JSC_HOST_CALL functionPrintValue(ExecState* exec)
{
    for (unsigned i = 0; i < exec->argumentCount(); ++i) {
        if (i)
            dataLog(" ");
        dataLog(exec->uncheckedArgument(i));
    }
    return JSValue::encode(jsUndefined());
}

void JSDollarVMPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    
    addFunction(vm, globalObject, "crash", functionCrash, 0);
    
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(&vm, "dfgTrue"), 0, functionDFGTrue, DFGTrueIntrinsic, DontEnum);
    
    addFunction(vm, globalObject, "llintTrue", functionLLintTrue, 0);
    addFunction(vm, globalObject, "jitTrue", functionJITTrue, 0);
    
    addFunction(vm, globalObject, "gc", functionGC, 0);
    addFunction(vm, globalObject, "edenGC", functionEdenGC, 0);
    
    addFunction(vm, globalObject, "codeBlockForFrame", functionCodeBlockForFrame, 1);
    addFunction(vm, globalObject, "printSourceFor", functionPrintSourceFor, 1);
    addFunction(vm, globalObject, "printByteCodeFor", functionPrintByteCodeFor, 1);
    
    addFunction(vm, globalObject, "print", functionPrint, 1);
    addFunction(vm, globalObject, "printCallFrame", functionPrintCallFrame, 0);
    addFunction(vm, globalObject, "printStack", functionPrintStack, 0);

    addFunction(vm, globalObject, "printValue", functionPrintValue, 1);
}

} // namespace JSC
