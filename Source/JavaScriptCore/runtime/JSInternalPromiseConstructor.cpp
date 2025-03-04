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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSInternalPromiseConstructor.h"

#include "JSCBuiltins.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSInternalPromise.h"
#include "JSInternalPromisePrototype.h"
#include "StructureInlines.h"

#include "JSInternalPromiseConstructor.lut.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSInternalPromiseConstructor);

const ClassInfo JSInternalPromiseConstructor::s_info = { "Function", &Base::s_info, &internalPromiseConstructorTable, CREATE_METHOD_TABLE(JSInternalPromiseConstructor) };

/* Source for JSInternalPromiseConstructor.lut.h
@begin internalPromiseConstructorTable
  internalAll  JSInternalPromiseConstructorFuncInternalAll DontEnum|Function 1
@end
*/

JSInternalPromiseConstructor* JSInternalPromiseConstructor::create(VM& vm, Structure* structure, JSInternalPromisePrototype* promisePrototype)
{
    JSInternalPromiseConstructor* constructor = new (NotNull, allocateCell<JSInternalPromiseConstructor>(vm.heap)) JSInternalPromiseConstructor(vm, structure);
    constructor->finishCreation(vm, promisePrototype);
    return constructor;
}

Structure* JSInternalPromiseConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSInternalPromiseConstructor::JSInternalPromiseConstructor(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

static EncodedJSValue JSC_HOST_CALL constructPromise(ExecState* exec)
{
    JSGlobalObject* globalObject = exec->callee()->globalObject();
    VM& vm = exec->vm();
    JSInternalPromise* promise = JSInternalPromise::create(vm, globalObject->internalPromiseStructure());
    promise->initialize(exec, globalObject, exec->argument(0));
    return JSValue::encode(promise);
}

ConstructType JSInternalPromiseConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructPromise;
    return ConstructTypeHost;
}

CallType JSInternalPromiseConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = constructPromise;
    return CallTypeHost;
}

bool JSInternalPromiseConstructor::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<Base>(exec, internalPromiseConstructorTable, jsCast<JSInternalPromiseConstructor*>(object), propertyName, slot);
}

} // namespace JSC
