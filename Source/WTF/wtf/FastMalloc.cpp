// Copyright (c) 2005, 2007, Google Inc. All rights reserved.

/*
 * Copyright (C) 2005-2009, 2011, 2015 Apple Inc. All rights reserved.
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
#include "FastMalloc.h"

#include "CheckedArithmetic.h"
#include "CurrentTime.h"
#include <limits>
#include <string.h>
#include <wtf/DataLog.h>

#if OS(WINDOWS)
#include <windows.h>
#elif OS(AROS) || OS(AMIGAOS4)
#else
#include <pthread.h>
#include <sys/resource.h>
#endif

#if OS(DARWIN)
#include <mach/mach_init.h>
#include <malloc/malloc.h>
#endif

namespace WTF {

void* fastZeroedMalloc(size_t n) 
{
    void* result = fastMalloc(n);
    if(result)
        memset(result, 0, n);
    return result;
}

char* fastStrDup(const char* src)
{
    size_t len = strlen(src) + 1;
    char* dup = static_cast<char*>(fastMalloc(len));
    memcpy(dup, src, len);
    return dup;
}

TryMallocReturnValue tryFastZeroedMalloc(size_t n) 
{
    void* result;
    if (!tryFastMalloc(n).getValue(result))
        return 0;
    memset(result, 0, n);
    return result;
}

} // namespace WTF

#if defined(USE_SYSTEM_MALLOC) && USE_SYSTEM_MALLOC

#if OS(WINDOWS)
#include <malloc.h>
#elif OS(AROS)|| OS(AMIGAOS4)
#include "mui/arosbailout.h"
#include "mui/execallocator.h"
#elif OS(MORPHOS)
#include <clib/debug_protos.h>
#endif

namespace WTF {

size_t fastMallocGoodSize(size_t bytes)
{
#if OS(DARWIN)
    return malloc_good_size(bytes);
#else
    return bytes;
#endif
}

#if OS(WINDOWS)

void* fastAlignedMalloc(size_t alignment, size_t size) 
{
    return _aligned_malloc(size, alignment);
}

void fastAlignedFree(void* p) 
{
    _aligned_free(p);
}

#else

void* fastAlignedMalloc(size_t alignment, size_t size) 
{
retry:
    void* p = nullptr;
#if OS(AROS) || OS(AMIGAOS4)
    p = allocator_getmem_aligned(size, alignment);
#else
    posix_memalign(&p, alignment, size ? size : 2);
#endif

    if (unlikely(!p))
    {
#if OS(AROS)
        if (aros_memory_allocation_error(size ? size : 2, alignment) == 1)
            goto retry;

        if (aros_is_memory_bailout())
            aros_bailout_jump();
#endif
#if OS(AMIGAOS)
        kprintf("fastAlignedMalloc: Failed to allocate %lu bytes. Happy crash sponsored by WebKit will follow.\n", n ? n : 2);
        if(amigaos_crash(size ? size : 2))
            goto retry;
#endif
#if OS(MORPHOS)
        kprintf("fastAlignedMalloc: Failed to allocate %lu bytes. Happy crash sponsored by WebKit will follow.\n", n ? n : 2);
        if(morphos_crash(size ? size : 2))
            goto retry;
#endif
    }
    return p;
}

void fastAlignedFree(void* p) 
{
#if OS(AROS) || OS(AMIGAOS4)
    allocator_freemem(p);
#else
    free(p);
#endif
}

#endif // OS(WINDOWS)

TryMallocReturnValue tryFastMalloc(size_t n) 
{
    return malloc(n ? n : 2);
}

void* fastMalloc(size_t n)
{
retry:
    void* result = malloc(n ? n : 2);
    if (unlikely(!result))
    {
#if OS(AROS)
        if (aros_memory_allocation_error(n ? n : 2, 4) == 1)
            goto retry;

        if (aros_is_memory_bailout())
            aros_bailout_jump();
#endif
#if OS(AMIGAOS)
        kprintf("fastAlignedMalloc: Failed to allocate %lu bytes. Happy crash sponsored by WebKit will follow.\n", n ? n : 2);
        if(amigaos_crash(size ? size : 2))
            goto retry;
#endif
#if OS(MORPHOS)
        kprintf("fastMalloc: Failed to allocate %lu bytes. Happy crash sponsored by WebKit will follow.\n", n ? n : 2);
        if(morphos_crash(n ? n : 2)) 
            goto retry;
#endif
    }

    return result;
}

TryMallocReturnValue tryFastCalloc(size_t n_elements, size_t element_size)
{
    return calloc(n_elements ? n_elements : 1, element_size ? element_size : 2);
}

void* fastCalloc(size_t n_elements, size_t element_size)
{
retry:
    void* result = calloc(n_elements ? n_elements : 1, element_size ? element_size : 2);
    if (unlikely(!result))
    {
#if OS(AROS)
        if (aros_memory_allocation_error((n_elements ? n_elements : 1)*(element_size ? element_size : 2), 4) == 1)
            goto retry;

        if (aros_is_memory_bailout())
            aros_bailout_jump();
#endif
#if OS(AMIGAOS)
        kprintf("fastAlignedMalloc: Failed to allocate %lu bytes. Happy crash sponsored by WebKit will follow.\n", n ? n : 2);
        if(amigaos_crash(size ? size : 2))
            goto retry;
#endif
#if OS(MORPHOS)
        kprintf("fastCalloc: Failed to allocate %lu x %lu bytes. Happy crash sponsored by WebKit will follow.\n", n_elements ? n_elements : 1, element_size ? element_size : 2);
        if(morphos_crash((n_elements ? n_elements : 1)*(element_size ? element_size : 2))) 
            goto retry;
#endif
    }

    return result;
}

void fastFree(void* p)
{
    free(p);
}

void* fastRealloc(void* p, size_t n)
{
retry:
    void* result = realloc(p, n ? n : 2);
    if (unlikely(!result))
    {
#if OS(AROS)
        if (aros_memory_allocation_error(n ? n : 2, 4) == 1)
            goto retry;

        if (aros_is_memory_bailout())
            aros_bailout_jump();
#endif
#if OS(AMIGAOS)
        kprintf("fastAlignedMalloc: Failed to allocate %lu bytes. Happy crash sponsored by WebKit will follow.\n", n ? n : 2);
        if(amigaos_crash(size ? size : 2))
            goto retry;
#endif
#if OS(MORPHOS)
        kprintf("fastRealloc: Failed to allocate %lu bytes. Happy crash sponsored by WebKit will follow.\n", n ? n : 2);
        if(morphos_crash(n ? n : 2))
            goto retry;
#endif
    }
    return result;
}

void releaseFastMallocFreeMemory() { }
void releaseFastMallocFreeMemoryForThisThread() { }
    
FastMallocStatistics fastMallocStatistics()
{
    FastMallocStatistics statistics = { 0, 0, 0 };
    return statistics;
}

size_t fastMallocSize(const void* p)
{
#if OS(DARWIN)
    return malloc_size(p);
#elif OS(WINDOWS)
    return _msize(const_cast<void*>(p));
#else
    UNUSED_PARAM(p);
    return 1;
#endif
}

} // namespace WTF

#else // defined(USE_SYSTEM_MALLOC) && USE_SYSTEM_MALLOC

#include <bmalloc/bmalloc.h>

namespace WTF {

void* fastMalloc(size_t size)
{
    return bmalloc::api::malloc(size);
}

void* fastCalloc(size_t numElements, size_t elementSize)
{
    Checked<size_t> checkedSize = elementSize;
    checkedSize *= numElements;
    void* result = fastZeroedMalloc(checkedSize.unsafeGet());
    if (!result)
        CRASH();
    return result;
}

void* fastRealloc(void* object, size_t size)
{
    return bmalloc::api::realloc(object, size);
}

void fastFree(void* object)
{
    bmalloc::api::free(object);
}

size_t fastMallocSize(const void*)
{
    // FIXME: This is incorrect; best fix is probably to remove this function.
    // Caller currently are all using this for assertion, not to actually check
    // the size of the allocation, so maybe we can come up with something for that.
    return 1;
}

size_t fastMallocGoodSize(size_t size)
{
    return size;
}

void* fastAlignedMalloc(size_t alignment, size_t size) 
{
    return bmalloc::api::memalign(alignment, size);
}

void fastAlignedFree(void* p) 
{
    bmalloc::api::free(p);
}

TryMallocReturnValue tryFastMalloc(size_t size)
{
    return bmalloc::api::tryMalloc(size);
}
    
TryMallocReturnValue tryFastCalloc(size_t numElements, size_t elementSize)
{
    Checked<size_t, RecordOverflow> checkedSize = elementSize;
    checkedSize *= numElements;
    if (checkedSize.hasOverflowed())
        return nullptr;
    return tryFastZeroedMalloc(checkedSize.unsafeGet());
}
    
void releaseFastMallocFreeMemoryForThisThread()
{
    bmalloc::api::scavengeThisThread();
}

void releaseFastMallocFreeMemory()
{
    bmalloc::api::scavenge();
}

FastMallocStatistics fastMallocStatistics()
{

    // FIXME: Can bmalloc itself report the stats instead of relying on the OS?
    FastMallocStatistics statistics;
    statistics.freeListBytes = 0;
    statistics.reservedVMBytes = 0;

#if OS(WINDOWS)
    PROCESS_MEMORY_COUNTERS resourceUsage;
    GetProcessMemoryInfo(GetCurrentProcess(), &resourceUsage, sizeof(resourceUsage));
    statistics.committedVMBytes = resourceUsage.PeakWorkingSetSize;
#else
    struct rusage resourceUsage;
    getrusage(RUSAGE_SELF, &resourceUsage);

#if OS(DARWIN)
    statistics.committedVMBytes = resourceUsage.ru_maxrss;
#else
    statistics.committedVMBytes = resourceUsage.ru_maxrss * 1024;
#endif // OS(DARWIN)

#endif // OS(WINDOWS)
    return statistics;
}

} // namespace WTF

#endif // defined(USE_SYSTEM_MALLOC) && USE_SYSTEM_MALLOC
