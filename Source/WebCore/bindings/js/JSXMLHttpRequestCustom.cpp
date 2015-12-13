/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSXMLHttpRequest.h"

#include "Blob.h"
#include "DOMFormData.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Event.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "InspectorInstrumentation.h"
#include "JSBlob.h"
#include "JSDOMFormData.h"
#include "JSDOMWindowCustom.h"
#include "JSDocument.h"
#include "JSEvent.h"
#include "JSEventListener.h"
#include "XMLHttpRequest.h"
#include <interpreter/StackVisitor.h>
#include <runtime/ArrayBuffer.h>
#include <runtime/Error.h>
#include <runtime/JSArrayBuffer.h>
#include <runtime/JSArrayBufferView.h>
#include <runtime/JSONObject.h>

using namespace JSC;

namespace WebCore {

void JSXMLHttpRequest::visitAdditionalChildren(SlotVisitor& visitor)
{
    if (XMLHttpRequestUpload* upload = impl().optionalUpload())
        visitor.addOpaqueRoot(upload);

    if (Document* responseDocument = impl().optionalResponseXML())
        visitor.addOpaqueRoot(responseDocument);

    if (ArrayBuffer* responseArrayBuffer = impl().optionalResponseArrayBuffer())
        visitor.addOpaqueRoot(responseArrayBuffer);

    if (Blob* responseBlob = impl().optionalResponseBlob())
        visitor.addOpaqueRoot(responseBlob);
}

// Custom functions
JSValue JSXMLHttpRequest::open(ExecState& state)
{
    if (state.argumentCount() < 2)
        return state.vm().throwException(&state, createNotEnoughArgumentsError(&state));

    const URL& url = impl().scriptExecutionContext()->completeURL(state.uncheckedArgument(1).toString(&state)->value(&state));
    String method = state.uncheckedArgument(0).toString(&state)->value(&state);

    ExceptionCode ec = 0;
    if (state.argumentCount() >= 3) {
        bool async = state.uncheckedArgument(2).toBoolean(&state);
        if (!state.argument(3).isUndefined()) {
            String user = valueToStringWithNullCheck(&state, state.uncheckedArgument(3));

            if (!state.argument(4).isUndefined()) {
                String password = valueToStringWithNullCheck(&state, state.uncheckedArgument(4));
                impl().open(method, url, async, user, password, ec);
            } else
                impl().open(method, url, async, user, ec);
        } else
            impl().open(method, url, async, ec);
    } else
        impl().open(method, url, ec);

    setDOMException(&state, ec);
    return jsUndefined();
}

class SendFunctor {
public:
    SendFunctor()
        : m_hasSkippedFirstFrame(false)
        , m_line(0)
        , m_column(0)
    {
    }

    unsigned line() const { return m_line; }
    unsigned column() const { return m_column; }
    String url() const { return m_url; }

    StackVisitor::Status operator()(StackVisitor& visitor)
    {
        if (!m_hasSkippedFirstFrame) {
            m_hasSkippedFirstFrame = true;
            return StackVisitor::Continue;
        }

        unsigned line = 0;
        unsigned column = 0;
        visitor->computeLineAndColumn(line, column);
        m_line = line;
        m_column = column;
        m_url = visitor->sourceURL();
        return StackVisitor::Done;
    }

private:
    bool m_hasSkippedFirstFrame;
    unsigned m_line;
    unsigned m_column;
    String m_url;
};

JSValue JSXMLHttpRequest::send(ExecState& state)
{
    InspectorInstrumentation::willSendXMLHttpRequest(impl().scriptExecutionContext(), impl().url());

    ExceptionCode ec = 0;
    JSValue val = state.argument(0);
    if (val.isUndefinedOrNull())
        impl().send(ec);
    else if (val.inherits(JSDocument::info()))
        impl().send(JSDocument::toWrapped(val), ec);
    else if (val.inherits(JSBlob::info()))
        impl().send(JSBlob::toWrapped(val), ec);
    else if (val.inherits(JSDOMFormData::info()))
        impl().send(JSDOMFormData::toWrapped(val), ec);
    else if (val.inherits(JSArrayBuffer::info()))
        impl().send(toArrayBuffer(val), ec);
    else if (val.inherits(JSArrayBufferView::info())) {
        RefPtr<ArrayBufferView> view = toArrayBufferView(val);
        impl().send(view.get(), ec);
    } else
        impl().send(val.toString(&state)->value(&state), ec);

    SendFunctor functor;
    state.iterate(functor);
    impl().setLastSendLineAndColumnNumber(functor.line(), functor.column());
    impl().setLastSendURL(functor.url());
    setDOMException(&state, ec);
    return jsUndefined();
}

JSValue JSXMLHttpRequest::responseText(ExecState& state) const
{
    ExceptionCode ec = 0;
    String text = impl().responseText(ec);
    if (ec) {
        setDOMException(&state, ec);
        return jsUndefined();
    }
    return jsOwnedStringOrNull(&state, text);
}

JSValue JSXMLHttpRequest::response(ExecState& state) const
{
    // FIXME: Use CachedAttribute for other types than JSON as well.
    if (m_response && impl().responseCacheIsValid())
        return m_response.get();

    if (!impl().doneWithoutErrors() && impl().responseTypeCode() > XMLHttpRequest::ResponseTypeText)
        return jsNull();

    switch (impl().responseTypeCode()) {
    case XMLHttpRequest::ResponseTypeDefault:
    case XMLHttpRequest::ResponseTypeText:
        return responseText(state);

    case XMLHttpRequest::ResponseTypeJSON:
        {
            JSValue value = JSONParse(&state, impl().responseTextIgnoringResponseType());
            if (!value)
                value = jsNull();
            JSXMLHttpRequest* jsRequest = const_cast<JSXMLHttpRequest*>(this);
            jsRequest->m_response.set(state.vm(), jsRequest, value);

            impl().didCacheResponseJSON();

            return value;
        }

    case XMLHttpRequest::ResponseTypeDocument:
        {
            ExceptionCode ec = 0;
            Document* document = impl().responseXML(ec);
            if (ec) {
                setDOMException(&state, ec);
                return jsUndefined();
            }
            return toJS(&state, globalObject(), document);
        }

    case XMLHttpRequest::ResponseTypeBlob:
        return toJS(&state, globalObject(), impl().responseBlob());

    case XMLHttpRequest::ResponseTypeArrayBuffer:
        return toJS(&state, globalObject(), impl().responseArrayBuffer());
    }

    return jsUndefined();
}

} // namespace WebCore
