/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO_TRACK)

#include "JSTextTrack.h"

#include "JSTextTrackCueList.h"
#include "JSTrackCustom.h"

using namespace JSC;

namespace WebCore {

void JSTextTrack::visitAdditionalChildren(SlotVisitor& visitor)
{
    visitor.addOpaqueRoot(root(&impl()));
}

void JSTextTrack::setKind(ExecState& state, JSValue value)
{
#if ENABLE(MEDIA_SOURCE)
    auto& string = value.toString(&state)->value(&state);
    if (state.hadException())
        return;
    impl().setKind(string);
#else
    UNUSED_PARAM(state);
    UNUSED_PARAM(value);
#endif
}

void JSTextTrack::setLanguage(ExecState& state, JSValue value)
{
#if ENABLE(MEDIA_SOURCE)
    auto& string = value.toString(&state)->value(&state);
    if (state.hadException())
        return;
    impl().setLanguage(string);
#else
    UNUSED_PARAM(state);
    UNUSED_PARAM(value);
#endif
}

} // namespace WebCore

#endif
