/*
 * Functionality related to thread wrapping
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "ggggc/gc.h"
#include "ggggc-internals.h"

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
    ThreadInfo ti = arg;
    GGC_PUSH_1(ti);

    ti->func(GGC_R(ti, arg));

    /* now remove this thread from the thread barrier */
    while (ggc_mutex_trylock(&ggggc_worldBarrierLock) != 0)
        GGC_YIELD();
    ggggc_threadCount--;
    if (ggggc_threadCount > 0) {
        ggc_barrier_destroy(&ggggc_worldBarrier);
        ggc_barrier_init(&ggggc_worldBarrier, NULL, ggggc_threadCount);
    }
    ggc_mutex_unlock(&ggggc_worldBarrierLock);

    return NULL;
}

#if GGGGC_THREADS_POSIX
#include "threads-posix.c"

#else
#error Unknown threading platform.

#endif
