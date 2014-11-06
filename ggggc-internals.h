/*
 * Header for internal GGGGC functions
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

#ifndef GGGGC_INTERNALS_H
#define GGGGC_INTERNALS_H 1

#include "ggggc/gc.h"

/* allocate an object in generation 0, collecting if impossible */
void *ggggc_mallocGen0(struct GGGGC_Descriptor *descriptor, int force);

/* allocate an object in the requested generation >= 0, returning NULL if impossible */
void *ggggc_mallocGen1(struct GGGGC_Descriptor *descriptor, unsigned char gen, int force);

/* heuristically expand a generation if it has too many survivors */
void ggggc_expandGeneration(struct GGGGC_Pool *pool);

/* run a collection */
void ggggc_collect(unsigned char gen);

/* global heuristic for "please stop the world" */
extern volatile int ggggc_stopTheWorld;

/* global world-stopping barrier */
extern ggc_barrier_t ggggc_worldBarrier;
extern size_t ggggc_threadCount;
extern ggc_mutex_t ggggc_worldBarrierLock;

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
extern struct GGGGC_Pool *ggggc_gens[GGGGC_GENERATIONS + 1];

/* and each have their own allocation pool */
extern struct GGGGC_Pool *ggggc_pools[GGGGC_GENERATIONS + 1];

/* descriptor descriptors */
extern struct GGGGC_Descriptor *ggggc_descriptorDescriptors[GGGGC_WORDS_PER_POOL/GGGGC_BITS_PER_WORD+sizeof(struct GGGGC_Descriptor)];

/* and a lock for the descriptor descriptors */
extern ggc_mutex_t ggggc_descriptorDescriptorsLock;

#endif
