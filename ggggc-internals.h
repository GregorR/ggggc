/*
 * Header for internal GGGGC functions
 *
 * Copyright (c) 2014, 2015 Gregor Richards
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

#ifndef GGGGC_INTERNALS_H
#define GGGGC_INTERNALS_H 1

#include "ggggc/gc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* allocate an object, collecting if impossible. Descriptor is for protection
 * only */
void *ggggc_mallocRaw(struct GGGGC_Descriptor **descriptor, ggc_size_t size);

/* allocate and initialize a pool */
struct GGGGC_Pool *ggggc_newPool(int mustSucceed);

/* allocate and initialize a pool, based on a prototype */
struct GGGGC_Pool *ggggc_newPoolProto(struct GGGGC_Pool *pool);

/* heuristically expand a pool list if it has too many survivors
 * poolList: Pool list to expand
 * newPool: Function to allocate a new pool based on a prototype pool
 * ratio: As a power of two, portion of pool that should be survivors */
void ggggc_expandPoolList(struct GGGGC_Pool *poolList,
                          struct GGGGC_Pool *(*newPool)(struct GGGGC_Pool *),
                          int ratio);

/* free a generation (used when a thread exits) */
void ggggc_freeGeneration(struct GGGGC_Pool *proto);

/* run a collection */
void ggggc_collect0(unsigned char gen);

/* ggggc_worldBarrierLock protects:
 *  ggggc_worldBarrier
 *  ggggc_threadCount
 *  ggggc_blockedThreadPool0s
 *  ggggc_blockedThreadPointerStacks
 *
 * It should be acquired to change any of these, and by the main thread during
 * collection, for the ENTIRE duration of collection
 */
extern ggc_mutex_t ggggc_worldBarrierLock;

/* global world-stopping barrier */
extern ggc_barrier_t ggggc_worldBarrier;

/* number of threads in the system */
extern ggc_size_t ggggc_threadCount;

/* during stop-the-world, need a queue of pools and pointer stacks to scan */
extern ggc_mutex_t ggggc_rootsLock;
struct GGGGC_PoolList {
    struct GGGGC_PoolList *next;
    struct GGGGC_Pool *pool;
};
extern struct GGGGC_PoolList *ggggc_rootPool0List;
struct GGGGC_PointerStackList {
    struct GGGGC_PointerStackList *next;
    struct GGGGC_PointerStack *pointerStack;
};
extern struct GGGGC_PointerStackList *ggggc_rootPointerStackList;

/* threads which are blocked need to store their roots and pools aside when they can't stop the world */
extern struct GGGGC_PoolList *ggggc_blockedThreadPool0s;
extern struct GGGGC_PointerStackList *ggggc_blockedThreadPointerStacks;

/* the generation 0 pools are thread-local */
extern ggc_thread_local struct GGGGC_Pool *ggggc_gen0;

/* the current allocation pool for generation 0 */
extern ggc_thread_local struct GGGGC_Pool *ggggc_pool0;

/* the later-generation pools are shared */
extern struct GGGGC_Pool *ggggc_gens[GGGGC_GENERATIONS];

/* and each have their own allocation pool */
extern struct GGGGC_Pool *ggggc_pools[GGGGC_GENERATIONS];

/* descriptor descriptors */
extern struct GGGGC_Descriptor *ggggc_descriptorDescriptors[GGGGC_WORDS_PER_POOL/GGGGC_BITS_PER_WORD+sizeof(struct GGGGC_Descriptor)];

/* and a lock for the descriptor descriptors */
extern ggc_mutex_t ggggc_descriptorDescriptorsLock;

/* list of pointers to search and associated macros */
#define TOSEARCH_SZ 1024
struct ToSearch {
    struct ToSearch *prev, *next;
    ggc_size_t used;
    void **buf;
};

#define TOSEARCH_INIT() do { \
    if (toSearchList.buf == NULL) { \
        toSearchList.buf = (void **) malloc(TOSEARCH_SZ * sizeof(void *)); \
        if (toSearchList.buf == NULL) { \
            /* FIXME: handle somehow? */ \
            perror("malloc"); \
            abort(); \
        } \
    } \
    toSearch = &toSearchList; \
    toSearch->used = 0; \
} while(0)
#define TOSEARCH_NEXT() do { \
    if (!toSearch->next) { \
        struct ToSearch *tsn = (struct ToSearch *) malloc(sizeof(struct ToSearch)); \
        toSearch->next = tsn; \
        tsn->prev = toSearch; \
        tsn->next = NULL; \
        tsn->buf = (void **) malloc(TOSEARCH_SZ * sizeof(void *)); \
        if (tsn->buf == NULL) { \
            perror("malloc"); \
            abort(); \
        } \
    } \
    toSearch = toSearch->next; \
    toSearch->used = 0; \
} while(0)
#define TOSEARCH_ADD(ptr) do { \
    if (toSearch->used >= TOSEARCH_SZ) TOSEARCH_NEXT(); \
    toSearch->buf[toSearch->used++] = (ptr); \
} while(0)
#define TOSEARCH_POP(type, into) do { \
    into = (type) toSearch->buf[--toSearch->used]; \
    if (toSearch->used == 0 && toSearch->prev) \
        toSearch = toSearch->prev; \
} while(0)

/* the shape for finalizer entries */
GGC_TYPE(GGGGC_FinalizerEntry)
    GGC_MPTR(GGGGC_FinalizerEntry, next);
    GGC_MPTR(void *, obj);
    GGC_MDATA(ggc_finalizer_t, finalizer);
GGC_END_TYPE(GGGGC_FinalizerEntry,
    GGC_PTR(GGGGC_FinalizerEntry, next)
    GGC_PTR(GGGGC_FinalizerEntry, obj)
    )

/* and function for running finalizers */
void ggggc_runFinalizers(GGGGC_FinalizerEntry finalizers);

#ifdef __cplusplus
}
#endif

#endif
