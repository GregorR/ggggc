#ifndef GGGGC_INTERNALS_H
#define GGGGC_INTERNALS_H 1

#include "ggggc/gc.h"

/* allocate an object in generation 0, collecting if impossible */
void *ggggc_mallocGen0(struct GGGGC_Descriptor *descriptor, int force);

/* allocate an object in the requested generation >= 0, returning NULL if impossible */
void *ggggc_mallocGen1(struct GGGGC_Descriptor *descriptor, unsigned char gen, int force);

/* run a collection */
void ggggc_collect(unsigned char gen);

/* number of GGGGC threads currently in the system */
extern size_t ggggc_threadCt;

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
