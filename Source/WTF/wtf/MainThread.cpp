/*
 * Copyright (C) 2007, 2008, 2015 Apple Inc. All rights reserved.
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
#include "MainThread.h"

#include "CurrentTime.h"
#include "Deque.h"
#include "StdLibExtras.h"
#include "Threading.h"
#include <mutex>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ThreadSpecific.h>

namespace WTF {

#if PLATFORM(MUI)
static long long g_functionid = 0;

struct Invocation {
    std::function<void ()> function;
    long long id;

    Invocation(std::function<void ()> function = nullptr)
        : function(function)
        , id(++g_functionid)
    {
    }
//    bool operator == (const Invocation& o)
//    {
//        return function == o.function;
//    }
};
//
//class FunctionWithContextFinder {
//public:
//    FunctionWithContextFinder(const FunctionWithContext& m) : m(m) {}
//    bool operator() (FunctionWithContext& o) { return o == m; }
//    FunctionWithContext m;
//};
#endif

static bool callbacksPaused; // This global variable is only accessed from main thread.
#if !OS(DARWIN) || PLATFORM(EFL) || PLATFORM(GTK)
static ThreadIdentifier mainThreadIdentifier;
#endif

static StaticLock mainThreadFunctionQueueMutex;

#if !PLATFORM(MUI)
static Deque<std::function<void ()>>& functionQueue()
{
    static NeverDestroyed<Deque<std::function<void ()>>> functionQueue;
    return functionQueue;
}
#else
static Deque<Invocation>& invocationQueue()
{
    static NeverDestroyed<Deque<Invocation>> invocationQueue;
    return invocationQueue;
}
#endif


#if !OS(DARWIN) || PLATFORM(EFL) || PLATFORM(GTK)

void initializeMainThread()
{
    static bool initializedMainThread;
    if (initializedMainThread)
        return;
    initializedMainThread = true;

    mainThreadIdentifier = currentThread();

    initializeMainThreadPlatform();
    initializeGCThreads();
}

#else

static pthread_once_t initializeMainThreadKeyOnce = PTHREAD_ONCE_INIT;

static void initializeMainThreadOnce()
{
    initializeMainThreadPlatform();
}

void initializeMainThread()
{
    pthread_once(&initializeMainThreadKeyOnce, initializeMainThreadOnce);
}

#if !USE(WEB_THREAD)
static void initializeMainThreadToProcessMainThreadOnce()
{
    initializeMainThreadToProcessMainThreadPlatform();
}

void initializeMainThreadToProcessMainThread()
{
    pthread_once(&initializeMainThreadKeyOnce, initializeMainThreadToProcessMainThreadOnce);
}
#else
static pthread_once_t initializeWebThreadKeyOnce = PTHREAD_ONCE_INIT;

static void initializeWebThreadOnce()
{
    initializeWebThreadPlatform();
}

void initializeWebThread()
{
    pthread_once(&initializeWebThreadKeyOnce, initializeWebThreadOnce);
}
#endif // !USE(WEB_THREAD)

#endif

// 0.1 sec delays in UI is approximate threshold when they become noticeable. Have a limit that's half of that.
static const auto maxRunLoopSuspensionTime = std::chrono::milliseconds(50);

void dispatchFunctionsFromMainThread()
{
    ASSERT(isMainThread());

    if (callbacksPaused)
        return;

    auto startTime = std::chrono::steady_clock::now();

    std::function<void ()> function;

    while (true) {
        {
            std::lock_guard<StaticLock> lock(mainThreadFunctionQueueMutex);
#if !PLATFORM(MUI)
            if (!functionQueue().size())
                break;

            function = functionQueue().takeFirst();
#else
            if (!invocationQueue().size())
                break;

            function = invocationQueue().takeFirst().function;
#endif
        }

        function();

        // If we are running accumulated functions for too long so UI may become unresponsive, we need to
        // yield so the user input can be processed. Otherwise user may not be able to even close the window.
        // This code has effect only in case the scheduleDispatchFunctionsOnMainThread() is implemented in a way that
        // allows input events to be processed before we are back here.
        if (std::chrono::steady_clock::now() - startTime > maxRunLoopSuspensionTime) {
            scheduleDispatchFunctionsOnMainThread();
            break;
        }
    }
}

void callOnMainThread(std::function<void ()> function)
{
    ASSERT(function);

    bool needToSchedule = false;

    {
        std::lock_guard<StaticLock> lock(mainThreadFunctionQueueMutex);
#if !PLATFORM(MUI)
        needToSchedule = functionQueue().size() == 0;
        functionQueue().append(WTF::move(function));
#else
        needToSchedule = invocationQueue().size() == 0;
        invocationQueue().append(WTF::move(function));
#endif
    }

    if (needToSchedule)
        scheduleDispatchFunctionsOnMainThread();
}

#if PLATFORM(MUI)
long long callOnMainThreadReturningJobID(std::function<void ()> function)
{
    ASSERT(function);
    long long id;

    bool needToSchedule = false;

    {
        std::lock_guard<StaticLock> lock(mainThreadFunctionQueueMutex);
        needToSchedule = invocationQueue().size() == 0;
        Invocation invocation(WTF::move(function));
        id = invocation.id;
        invocationQueue().append(invocation);
    }

    if (needToSchedule)
        scheduleDispatchFunctionsOnMainThread();

    return id;
}

void removeFromMainThreadByJobID(long long id)
{
    bool needToSchedule = false;
    {
        std::lock_guard<StaticLock> lock(mainThreadFunctionQueueMutex);

        Deque<Invocation>::const_iterator end = invocationQueue().end();
        for (Deque<Invocation>::const_iterator it = invocationQueue().begin(); it != end; ++it)
        {
            if(it->id == id)
            {
                invocationQueue().remove(it);
                break;
            }
        }

        needToSchedule = invocationQueue().size() == 0;
    }
    if (needToSchedule)
        scheduleDispatchFunctionsOnMainThread();
}
#endif

//void cancelCallOnMainThread(MainThreadFunction* function, void* context)
//{
//    ASSERT(function);
//
//    std::lock_guard<std::mutex> lock(mainThreadFunctionQueueMutex());
//
//    FunctionWithContextFinder pred(FunctionWithContext(function, context));
//
//    while (true) {
//        // We must redefine 'i' each pass, because the itererator's operator=
//        // requires 'this' to be valid, and remove() invalidates all iterators
//        FunctionQueue::iterator i(functionQueue().findIf(pred));
//        if (i == functionQueue().end())
//            break;
//        functionQueue().remove(i);
//    }
//}

void setMainThreadCallbacksPaused(bool paused)
{
    ASSERT(isMainThread());

    if (callbacksPaused == paused)
        return;

    callbacksPaused = paused;

    if (!callbacksPaused)
        scheduleDispatchFunctionsOnMainThread();
}

#if !OS(DARWIN) || PLATFORM(EFL) || PLATFORM(GTK)
bool isMainThread()
{
    return currentThread() == mainThreadIdentifier;
}
#endif

#if !USE(WEB_THREAD)
bool canAccessThreadLocalDataForThread(ThreadIdentifier threadId)
{
    return threadId == currentThread();
}
#endif

static ThreadSpecific<bool>* isGCThread;

void initializeGCThreads()
{
    isGCThread = new ThreadSpecific<bool>();
}

void registerGCThread()
{
    if (!isGCThread) {
        // This happens if we're running in a process that doesn't care about
        // MainThread.
        return;
    }

    **isGCThread = true;
}

bool isMainThreadOrGCThread()
{
    if (isGCThread->isSet() && **isGCThread)
        return true;

    return isMainThread();
}

} // namespace WTF
