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

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100

#include "config.h"
#include "WheelEventDeltaFilterMac.h"
#include "FloatPoint.h"

#import "NSScrollingInputFilterSPI.h"
#import <wtf/CurrentTime.h>

namespace WebCore {

WheelEventDeltaFilterMac::WheelEventDeltaFilterMac()
    : WheelEventDeltaFilter()
    , m_predominantAxisFilter(adoptNS([[_NSScrollingPredominantAxisFilter alloc] init]))
{
}

void WheelEventDeltaFilterMac::beginFilteringDeltas()
{
    m_beginFilteringDeltasTime = monotonicallyIncreasingTime();
    m_isFilteringDeltas = true;
}

void WheelEventDeltaFilterMac::updateFromDelta(const FloatSize& delta)
{
    if (!m_isFilteringDeltas)
        return;

    NSPoint filteredDeltaResult;
    NSPoint filteredVelocityResult;
    [m_predominantAxisFilter filterInputDelta:NSPoint(FloatPoint(delta.width(), delta.height())) timestamp:monotonicallyIncreasingTime() - m_beginFilteringDeltasTime outputDelta:&filteredDeltaResult velocity:&filteredVelocityResult];
    m_currentFilteredDelta = FloatSize(filteredDeltaResult.x, filteredDeltaResult.y);
}

void WheelEventDeltaFilterMac::endFilteringDeltas()
{
    m_currentFilteredDelta = FloatSize(0, 0);
    m_beginFilteringDeltasTime = 0;
    [m_predominantAxisFilter reset];
    m_isFilteringDeltas = false;
}

}

#endif /* PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100 */
