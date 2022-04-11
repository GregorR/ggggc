/*
 * Functionality related to thread wrapping
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "ggggc/gc.h"
#include "ggggc-internals.h"

#ifdef __cplusplus
extern "C" {
#endif

/* general purpose thread info */
typedef void (*ggggc_threadFunc)(ThreadArg arg);
GGC_TYPE(ThreadInfo)
    GGC_MDATA(ggggc_threadFunc, func);
    GGC_MPTR(ThreadArg, arg);
GGC_END_TYPE(ThreadInfo,
    GGC_PTR(ThreadInfo, arg)
    )

/* general purpose thread wrapper */
static void *ggggcThreadWrapper(void *arg)
{
    ThreadInfo ti = (ThreadInfo) arg;
    GGC_PUSH_1(ti);

    GGC_RD(ti, func)(GGC_RP(ti, arg));

    /* now remove this thread from the thread barrier */
    while (ggc_mutex_trylock(&ggggc_worldBarrierLock) != 0)
        GGC_YIELD();
    ggggc_threadCount--;
    if (ggggc_threadCount > 0) {
        ggc_barrier_destroy(&ggggc_worldBarrier);
        ggc_barrier_init(&ggggc_worldBarrier, ggggc_threadCount);
    }
    ggc_mutex_unlock(&ggggc_worldBarrierLock);

    /* and give back its pools */
    ggggc_freeGeneration(ggggc_gen0);

    return 0;
}

static ggc_thread_local struct GGGGC_PoolList blockedPoolListNode;
static ggc_thread_local struct GGGGC_PointerStackList blockedPointerStackListNode;

/* call this before blocking */
void ggc_pre_blocking()
{
    /* get a lock on the thread count etc */
    while (ggc_mutex_trylock(&ggggc_worldBarrierLock) != 0)
        GGC_YIELD();

    /* take ourselves out of contention */
    ggggc_threadCount--;
    if (ggggc_threadCount > 0) {
        ggc_barrier_destroy(&ggggc_worldBarrier);
        ggc_barrier_init(&ggggc_worldBarrier, ggggc_threadCount);
    }

    /* add our roots and pools */
    blockedPoolListNode.pool = ggggc_gen0;
    blockedPoolListNode.next = ggggc_blockedThreadPool0s;
    ggggc_blockedThreadPool0s = &blockedPoolListNode;
    blockedPointerStackListNode.pointerStack = ggggc_pointerStack;
    blockedPointerStackListNode.next = ggggc_blockedThreadPointerStacks;
    ggggc_blockedThreadPointerStacks = &blockedPointerStackListNode;

    ggc_mutex_unlock(&ggggc_worldBarrierLock);
}

/* and this after */
void ggc_post_blocking()
{
    struct GGGGC_PoolList *plCur;
    struct GGGGC_PointerStackList *pslCur;

    /* get a lock on the thread count etc */
    while (ggc_mutex_trylock(&ggggc_worldBarrierLock) != 0);
    /* FIXME: can't yield here, as yielding waits for stop-the-world if
     * applicable. Perhaps something would be more ideal than a spin-loop
     * though. */

    /* add ourselves back to the world barrier */
    ggc_barrier_destroy(&ggggc_worldBarrier);
    ggc_barrier_init(&ggggc_worldBarrier, ++ggggc_threadCount);

    /* remove our roots and pools from the list */
    if (ggggc_blockedThreadPool0s == &blockedPoolListNode) {
        ggggc_blockedThreadPool0s = ggggc_blockedThreadPool0s->next;

    } else {
        for (plCur = ggggc_blockedThreadPool0s; plCur->next; plCur = plCur->next) {
            if (plCur->next == &blockedPoolListNode) {
                plCur->next = plCur->next->next;
                break;
            }
        }

    }
    if (ggggc_blockedThreadPointerStacks == &blockedPointerStackListNode) {
        ggggc_blockedThreadPointerStacks = ggggc_blockedThreadPointerStacks->next;

    } else {
        for (pslCur = ggggc_blockedThreadPointerStacks; pslCur->next; pslCur = pslCur->next) {
            if (pslCur->next == &blockedPointerStackListNode) {
                pslCur->next = pslCur->next->next;
                break;
            }
        }

    }

    ggc_mutex_unlock(&ggggc_worldBarrierLock);
}

#if defined(GGGGC_THREADS_POSIX)
#include "threads/posix.c"

#elif defined(GGGGC_THREADS_MACOSX)
#include "threads/macosx.c"

#elif defined(GGGGC_THREADS_WINDOWS)
#include "threads/windows.c"

#elif !defined(GGGGC_NO_THREADS)
#error Unknown threading platform.

#endif

#ifdef __cplusplus
}
#endif
