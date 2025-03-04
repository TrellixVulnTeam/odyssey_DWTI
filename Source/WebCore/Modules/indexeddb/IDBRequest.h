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

#ifndef IDBRequest_h
#define IDBRequest_h

#if ENABLE(INDEXED_DATABASE)

#include "ActiveDOMObject.h"
#include "DOMError.h"
#include "DOMStringList.h"
#include "Event.h"
#include "EventListener.h"
#include "EventTarget.h"
#include "IDBAny.h"
#include "IDBCursor.h"
#include "ScriptWrappable.h"

namespace WebCore {

class IDBTransaction;

typedef int ExceptionCode;

class IDBRequest : public ScriptWrappable, public EventTargetWithInlineData, public ActiveDOMObject {
public:
    virtual ~IDBRequest() { }

    virtual RefPtr<IDBAny> result(ExceptionCode&) const = 0;
    virtual unsigned short errorCode(ExceptionCode&) const = 0;
    virtual RefPtr<DOMError> error(ExceptionCode&) const = 0;
    virtual RefPtr<IDBAny> source() const = 0;
    virtual RefPtr<IDBTransaction> transaction() const = 0;

    // Defined in the IDL
    enum ReadyState {
        PENDING = 1,
        DONE = 2,
        EarlyDeath = 3
    };

    virtual const String& readyState() const = 0;

protected:
    IDBRequest(ScriptExecutionContext*);
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

#endif // LegacyRequest_h
