#include "ggggc-internals.h"

/* publics */
ggc_thread_local struct GGGGC_PointerStack *ggggc_pointerStack, *ggggc_pointerStackGlobals;

/* internals */
volatile int ggggc_stopTheWorld;
ggc_barrier_t ggggc_worldBarrier;
ggc_size_t ggggc_threadCount;
ggc_mutex_t ggggc_worldBarrierLock = GGC_MUTEX_INITIALIZER;
ggc_mutex_t ggggc_rootsLock = GGC_MUTEX_INITIALIZER;
struct GGGGC_PoolList *ggggc_rootPool0List;
struct GGGGC_PointerStackList *ggggc_rootPointerStackList;
struct GGGGC_PoolList *ggggc_blockedThreadPool0s;
struct GGGGC_PointerStackList *ggggc_blockedThreadPointerStacks;
ggc_thread_local struct GGGGC_Pool *ggggc_gen0;
ggc_thread_local struct GGGGC_Pool *ggggc_pool0;
struct GGGGC_Pool *ggggc_gens[GGGGC_GENERATIONS];
struct GGGGC_Pool *ggggc_pools[GGGGC_GENERATIONS];
struct GGGGC_Descriptor *ggggc_descriptorDescriptors[GGGGC_WORDS_PER_POOL/GGGGC_BITS_PER_WORD+sizeof(struct GGGGC_Descriptor)];
ggc_mutex_t ggggc_descriptorDescriptorsLock;
