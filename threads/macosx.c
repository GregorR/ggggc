/*
 * Thread support for Mac OS X's awful almost-pthreads
 *
 * Copyright (c) 2014-2022 Gregor Richards
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

#define BLOCKING(func, call) \
int func \
{ \
    int ret, err; \
    ggc_pre_blocking(); \
    err = call; \
    ggc_post_blocking(); \
    ret = err ? -1 : 0; \
    errno = err; \
    return ret; \
}

BLOCKING(
    ggc_mutex_lock(ggc_mutex_t *mutex),
    pthread_mutex_lock(mutex)
)

int ggc_sem_destroy(ggc_sem_t *sem)
{
    dispatch_release(*sem);
    return 0;
}

int ggc_sem_init(ggc_sem_t *sem, unsigned int value)
{
    *sem = dispatch_semaphore_create(value);
    return (*sem ? 0 : -1);
}

int ggc_sem_post(ggc_sem_t *sem)
{
    dispatch_semaphore_signal(*sem);
    return 0;
}

BLOCKING(
    ggc_sem_wait(ggc_sem_t *sem),
    ggc_sem_wait_raw(sem)
)

int ggc_sem_wait_raw(ggc_sem_t *sem)
{
    return dispatch_semaphore_wait(*sem, DISPATCH_TIME_FOREVER);
}

int ggc_thread_create(
        ggc_thread_t *thread,
        void (*func)(GGC_ThreadArg),
        GGC_ThreadArg arg
) {
    ThreadInfo ti = NULL;

    GGC_PUSH_2(arg, ti);

    /* set up its thread info */
    ti = GGC_NEW(ThreadInfo);
    GGC_WD(ti, func, func);
    GGC_WP(ti, arg, arg);

    /* update our thread barrier */
    while (ggc_mutex_trylock(&ggggc_worldBarrierLock) != 0)
        GGC_YIELD();
    if (ggggc_threadCount == (ggc_size_t) -1) ggggc_threadCount = 1;
    else ggc_barrier_destroy(&ggggc_worldBarrier);
    ggc_barrier_init(&ggggc_worldBarrier, ++ggggc_threadCount);
    ggc_mutex_unlock(&ggggc_worldBarrierLock);

    /* spawn the pthread */
    if ((errno = pthread_create(thread, NULL, ggggcThreadWrapper, ti)))
        return -1;

    return 0;
}

BLOCKING(
    ggc_thread_join(ggc_thread_t thread),
    pthread_join(thread, NULL)
)

#include "gen-barriers.c"
