/*
 * Thread functionality for Windows
 *
 * Copyright (c) 2014 Gregor Richards
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

static HANDLE lockInitLock;
#if defined(__GNUC__)
static void __attribute__((constructor)) lockInitLockConstructor() {
#elif defined(__cplusplus)
class GGGGC_LockInitLockConstructor {
    public:
    GGGGC_LockInitLockConstructor() {
#else
#error No means of global construction: Please compile ggggc as C++.
#endif
        lockInitLock = CreateMutex(NULL, 0, NULL);
    }
#if defined(__cplusplus)
};
#endif

#define BLOCKING(func, call) \
int func \
{ \
    BOOL ret; \
    ggc_pre_blocking(); \
    ret = call; \
    ggc_post_blocking(); \
    errno = ret ? 0 : ENOMEM; \
    return ret ? 0 : -1; \
}

static void initLock(ggc_mutex_t *mutex)
{
    WaitForSingleObject(lockInitLock, INFINITE);
    *mutex = CreateMutex(NULL, 0, NULL);
    ReleaseMutex(lockInitLock);
}

BLOCKING(
    ggc_mutex_lock(ggc_mutex_t *mutex),
    WaitForSingleObject(*(mutex), INFINITE)
)

int ggc_mutex_lock_raw(ggc_mutex_t *mutex)
{
    if (!*mutex) initLock(mutex);
    return (WaitForSingleObject(*mutex, INFINITE) == WAIT_FAILED);
}

int ggc_mutex_trylock(ggc_mutex_t *mutex)
{
    if (!*mutex) initLock(mutex);
    return (WaitForSingleObject(*mutex, 0) == WAIT_FAILED);
}

int ggc_thread_create(ggc_thread_t *thread, void (*func)(ThreadArg), ThreadArg arg)
{
    ThreadInfo ti = NULL;

    GGC_PUSH_2(arg, ti);

    /* set up its thread info */
    ti = GGC_NEW(ThreadInfo);
    GGC_WD(ti, func, func);
    GGC_WP(ti, arg, arg);

    /* update our thread barrier */
    while (ggc_mutex_trylock(&ggggc_worldBarrierLock) != 0)
        GGC_YIELD();
    if (ggggc_threadCount == 0) ggggc_threadCount++;
    else ggc_barrier_destroy(&ggggc_worldBarrier);
    ggc_barrier_init(&ggggc_worldBarrier, ++ggggc_threadCount);
    ggc_mutex_unlock(&ggggc_worldBarrierLock);

    /* spawn the thread */
    *thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) ggggcThreadWrapper, ti, 0, NULL);
    if (!*thread)
        return -1;

    return 0;
}

BLOCKING(
    ggc_thread_join(ggc_thread_t thread),
    WaitForSingleObject(thread, INFINITE)
)

#include "gen-barriers.c"
