/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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
#include "FTLInlineCacheSize.h"

#if ENABLE(FTL_JIT)

#include "DFGNode.h"
#include "JITInlineCacheGenerator.h"
#include "MacroAssembler.h"

namespace JSC { namespace FTL {

using namespace DFG;

// The default sizes are x86-64-specific, and were found empirically. They have to cover the worst
// possible combination of registers leading to the largest possible encoding of each instruction in
// the IC.
//
// FIXME: The ARM64 sizes are overestimates; if there is any branch compaction then we should be able
// to get away with less. The branch compaction code currently validates the size of the IC before
// doing any compaction, so we need to overestimate and give the uncompacted size. This would be
// relatively easy to fix.
// https://bugs.webkit.org/show_bug.cgi?id=129335

size_t sizeOfGetById()
{
#if CPU(ARM64)
    return 36;
#else
    return 30;
#endif
}

size_t sizeOfPutById()
{
#if CPU(ARM64)
    return 44;
#else
    return 32;
#endif
}

size_t sizeOfCall()
{
#if CPU(ARM64)
    return 60;
#else
    return 60;
#endif
}

size_t sizeOfCallVarargs()
{
#if CPU(ARM64)
    return 332;
#else
    return 275;
#endif
}

size_t sizeOfCallForwardVarargs()
{
#if CPU(ARM64)
    return 312;
#else
    return 262;
#endif
}

size_t sizeOfConstructVarargs()
{
    return sizeOfCallVarargs(); // Should be the same size.
}

size_t sizeOfConstructForwardVarargs()
{
    return sizeOfCallForwardVarargs(); // Should be the same size.
}

size_t sizeOfIn()
{
#if CPU(ARM64)
    return 4;
#else
    return 5; 
#endif
}

size_t sizeOfICFor(Node* node)
{
    switch (node->op()) {
    case GetById:
        return sizeOfGetById();
    case PutById:
        return sizeOfPutById();
    case Call:
    case Construct:
        return sizeOfCall();
    case CallVarargs:
        return sizeOfCallVarargs();
    case CallForwardVarargs:
        return sizeOfCallForwardVarargs();
    case ConstructVarargs:
        return sizeOfConstructVarargs();
    case ConstructForwardVarargs:
        return sizeOfConstructForwardVarargs();
    case In:
        return sizeOfIn();
    default:
        return 0;
    }
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

