/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSDOMFormData.h"

#include "DOMFormData.h"
#include "HTMLFormElement.h"
#include "JSBlob.h"
#include "JSHTMLFormElement.h"
#include <runtime/Error.h>

using namespace JSC;

namespace WebCore {

static HTMLFormElement* toHTMLFormElementOrNull(JSC::JSValue value)
{
    return value.inherits(JSHTMLFormElement::info()) ? &jsCast<JSHTMLFormElement*>(asObject(value))->impl() : nullptr;
}

EncodedJSValue JSC_HOST_CALL constructJSDOMFormData(ExecState* exec)
{
    DOMConstructorObject* jsConstructor = jsCast<DOMConstructorObject*>(exec->callee());

    HTMLFormElement* form = toHTMLFormElementOrNull(exec->argument(0));
    RefPtr<DOMFormData> domFormData = DOMFormData::create(form);
    return JSValue::encode(asObject(toJS(exec, jsConstructor->globalObject(), domFormData.get())));
}

JSValue JSDOMFormData::append(ExecState& state)
{
    if (state.argumentCount() >= 2) {
        String name = state.argument(0).toString(&state)->value(&state);
        JSValue value = state.argument(1);
        if (value.inherits(JSBlob::info())) {
            String filename;
            if (state.argumentCount() >= 3 && !state.argument(2).isUndefinedOrNull())
                filename = state.argument(2).toString(&state)->value(&state);
            impl().append(name, JSBlob::toWrapped(value), filename);
        } else
            impl().append(name, value.toString(&state)->value(&state));
    }

    return jsUndefined();
}

} // namespace WebCore
