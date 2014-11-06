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
