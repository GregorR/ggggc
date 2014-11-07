/*
 * The collector
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "ggggc/gc.h"
#include "ggggc-internals.h"

/* list of pointers to search and associated macros */
struct ToSearch {
    size_t sz, used;
    void **buf;
};

#define TOSEARCH_INIT() do { \
    if (toSearch.buf == NULL) { \
        toSearch.sz = 128; \
        toSearch.used = 0; \
        toSearch.buf = malloc(toSearch.sz * sizeof(void *)); \
        if (toSearch.buf == NULL) { \
            /* FIXME: handle somehow? */ \
            perror("malloc"); \
            exit(1); \
        } \
    } \
} while(0)
#define TOSEARCH_EXPAND() do { \
    toSearch.sz *= 2; \
    toSearch.buf = realloc(toSearch.buf, toSearch.sz * sizeof(size_t)); \
    if (toSearch.buf == NULL) { \
        /* FIXME: handle somehow? */ \
        perror("realloc"); \
        exit(1); \
    } \
} while(0)
#define TOSEARCH_ADD(ptr) do { \
    if (toSearch.used >= toSearch.sz) TOSEARCH_EXPAND(); \
    toSearch.buf[toSearch.used++] = (ptr); \
} while(0)
#define TOSEARCH_POP() (toSearch.buf[--toSearch.used])

/* follow a forwarding pointer to the object it actually represents */
#define IS_FORWARDED_OBJECT(obj) (((size_t) (obj)->descriptor__ptr) & 1)
#define FOLLOW_FORWARDED_OBJECT(obj) do { \
    while (IS_FORWARDED_OBJECT(obj)) \
        obj = (struct GGGGC_Header *) (((size_t) (obj)->descriptor__ptr) & (size_t) ~1); \
} while(0)

/* same for descriptors as a special case of objects */
#define IS_FORWARDED_DESCRIPTOR(d) IS_FORWARDED_OBJECT(&(d)->header)
#define FOLLOW_FORWARDED_DESCRIPTOR(d) do { \
    struct GGGGC_Header *dobj = &(d)->header; \
    FOLLOW_FORWARDED_OBJECT(dobj); \
    d = (struct GGGGC_Descriptor *) dobj; \
} while(0)

/* macro to add an object's pointers to the tosearch list */
#define ADD_OBJECT_POINTERS(obj) do { \
    void **objVp = (void **) (obj); \
    struct GGGGC_Descriptor *descriptor = objVp[0]; \
    size_t curWord, curDescription = 0, curDescriptorWord; \
    FOLLOW_FORWARDED_DESCRIPTOR(descriptor); \
    if (descriptor->pointers[0] & 1) { \
        /* it has pointers */ \
        curDescriptorWord = 0; \
        for (curWord = 0; curWord < descriptor->size; curWord++) { \
            if (curWord % GGGGC_BITS_PER_WORD == 0) \
                curDescription = descriptor->pointers[curDescriptorWord++]; \
            if (curDescription & 1) \
                /* it's a pointer */ \
                TOSEARCH_ADD(&objVp[curWord]); \
            curDescription >>= 1; \
        } \
    } else { \
        /* no pointers other than the descriptor */ \
        TOSEARCH_ADD(&objVp[0]); \
    } \
} while(0)

static struct ToSearch toSearch;

/* run a collection */
void ggggc_collect(unsigned char gen)
{
    struct GGGGC_PoolList pool0Node, *plCur;
    struct GGGGC_Pool *poolCur;
    struct GGGGC_PointerStackList pointerStackNode, *pslCur;
    struct GGGGC_PointerStack *psCur;
    unsigned char genCur;
    size_t i;

    TOSEARCH_INIT();

    /* first, make sure we stop the world */
    while (ggc_mutex_trylock(&ggggc_worldBarrierLock) != 0) {
        /* somebody else is collecting */
        GGC_YIELD();
        return;
    }

    /* if nobody ever initialized the barrier, do so */
    if (ggggc_threadCount == 0) {
        ggggc_threadCount = 1;
        ggc_barrier_init(&ggggc_worldBarrier, NULL, ggggc_threadCount);
    }

    /* initialize our roots */
    ggc_mutex_lock_raw(&ggggc_rootsLock);
    pool0Node.pool = ggggc_gen0;
    pool0Node.next = ggggc_blockedThreadPool0s;
    ggggc_rootPool0List = &pool0Node;
    pointerStackNode.pointerStack = ggggc_pointerStack;
    pointerStackNode.next = ggggc_blockedThreadPointerStacks;
    ggggc_rootPointerStackList = &pointerStackNode;
    ggc_mutex_unlock(&ggggc_rootsLock);

    /* stop the world */
    ggggc_stopTheWorld = 1;
    ggc_barrier_wait(&ggggc_worldBarrier);
    ggggc_stopTheWorld = 0;

    /* wait for them to fill roots */
    ggc_barrier_wait(&ggggc_worldBarrier);

    /************************************************************
     * COLLECTION
     ***********************************************************/
collect:

    /* add our roots to the to-search list */
    for (pslCur = ggggc_rootPointerStackList; pslCur; pslCur = pslCur->next) {
        for (psCur = pslCur->pointerStack; psCur; psCur = psCur->next) {
            for (i = 0; i < psCur->size; i++) {
                TOSEARCH_ADD(psCur->pointers[i]);
            }
        }
    }

    /* add our remembered sets to the to-search list */
    for (genCur = gen + 1; genCur < GGGGC_GENERATIONS; genCur++) {
        for (poolCur = ggggc_gens[genCur]; poolCur; poolCur = poolCur->next) {
            for (i = 0; i < GGGGC_CARDS_PER_POOL; i++) {
                if (poolCur->remember[i]) {
                    struct GGGGC_Header *obj = (struct GGGGC_Header *)
                        ((size_t) poolCur + i * GGGGC_CARD_BYTES + poolCur->firstObject[i] * sizeof(size_t));
                    while (GGGGC_CARD_OF(obj) == i) {
                        ADD_OBJECT_POINTERS(obj);
                        obj = (struct GGGGC_Header *)
                            ((size_t) obj + obj->descriptor__ptr->size * sizeof(size_t));
                        if (obj->descriptor__ptr == NULL) break;
                    }
                }
            }
        }
    }

    /* now test all our pointers */
    while (toSearch.used) {
        void **ptr = TOSEARCH_POP();
        struct GGGGC_Header *obj = *ptr;
        if (obj == NULL) continue;

#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
        /* check for pre-corruption */
        if (obj->ggggc_memoryCorruptionCheck != GGGGC_MEMORY_CORRUPTION_VAL) {
            fprintf(stderr, "GGGGC: Memory corruption (precheck)!\n");
            abort();
        }
#endif

        /* is the object already forwarded? */
        if (IS_FORWARDED_OBJECT(obj)) {
            FOLLOW_FORWARDED_OBJECT(obj);
            *ptr = obj;
        }

        /* does it need to be moved? */
        if (GGGGC_GEN_OF(obj) <= gen) {
            struct GGGGC_Header *nobj;
            struct GGGGC_Descriptor *descriptor = obj->descriptor__ptr;
            FOLLOW_FORWARDED_DESCRIPTOR(descriptor);

            /* mark it as surviving */
            GGGGC_POOL_OF(obj)->survivors += descriptor->size;

            /* allocate in the new generation */
            nobj = ggggc_mallocGen1(descriptor, gen + 1, (gen == GGGGC_GENERATIONS));
            if (!nobj) {
                /* failed to allocate, need to collect gen+1 too */
                gen += 1;
                toSearch.used = 0;
                goto collect;
            }

            /* copy to the new object */
            memcpy(nobj, obj, descriptor->size * sizeof(size_t));

            /* mark it as forwarded */
            obj->descriptor__ptr = (struct GGGGC_Descriptor *) (((size_t) nobj) | 1);
            *ptr = obj = nobj;

            /* and add its pointers */
            ADD_OBJECT_POINTERS(obj);
        }

#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
        /* check for post-corruption */
        if (obj->ggggc_memoryCorruptionCheck != GGGGC_MEMORY_CORRUPTION_VAL) {
            fprintf(stderr, "GGGGC: Memory corruption (postcheck)!\n");
            abort();
        }
#endif
    }

    /* heuristically expand too-small generations */
    for (plCur = ggggc_rootPool0List; plCur; plCur = plCur->next)
        ggggc_expandGeneration(plCur->pool);
    for (genCur = 1; genCur <= gen; genCur++)
        ggggc_expandGeneration(ggggc_gens[genCur]);

    /* now clear out all the generations */
    for (plCur = ggggc_rootPool0List; plCur; plCur = plCur->next) {
        for (poolCur = plCur->pool; poolCur; poolCur = poolCur->next) {
            poolCur->free = poolCur->start;
        }
    }
    ggggc_pool0 = ggggc_gen0;
    for (genCur = 1; genCur < gen; genCur++) {
        for (poolCur = ggggc_gens[genCur]; poolCur; poolCur = poolCur->next) {
            memset(poolCur->remember, 0, GGGGC_CARDS_PER_POOL);
            poolCur->free = poolCur->start;
        }
        ggggc_pools[genCur] = ggggc_gens[genCur];
    }

    /* and the remembered sets */
    for (poolCur = ggggc_gens[gen+1]; poolCur; poolCur = poolCur->next)
        memset(poolCur->remember, 0, GGGGC_CARDS_PER_POOL);

    /* if we're collecting the last generation, we act like two-space copying */
    if (gen == GGGGC_GENERATIONS - 1) {
        struct GGGGC_Pool *from = ggggc_gens[gen];
        struct GGGGC_Pool *to = ggggc_gens[gen+1];

        /* swap them */
        ggggc_gens[gen] = to;
        ggggc_gens[gen+1] = from;

        /* and relabel them */
        for (poolCur = to; poolCur; poolCur = poolCur->next)
            poolCur->gen = gen;
        for (poolCur = from; poolCur; poolCur = poolCur->next)
            poolCur->gen = gen + 1;
    }

    /* free the other threads */
    ggc_barrier_wait(&ggggc_worldBarrier);
    ggc_mutex_unlock(&ggggc_worldBarrierLock);
}

/* explicitly yield to the collector */
void ggggc_yield()
{
    struct GGGGC_PoolList pool0Node;
    struct GGGGC_PointerStackList pointerStackNode;

    if (ggggc_stopTheWorld) {
        /* wait for the barrier once to stop the world */
        ggc_barrier_wait(&ggggc_worldBarrier);

        /* feed it my globals */
        ggc_mutex_lock_raw(&ggggc_rootsLock);
        pool0Node.pool = ggggc_gen0;
        pool0Node.next = ggggc_rootPool0List;
        ggggc_rootPool0List = &pool0Node;
        pointerStackNode.pointerStack = ggggc_pointerStack;
        pointerStackNode.next = ggggc_rootPointerStackList;
        ggggc_rootPointerStackList = &pointerStackNode;
        ggc_mutex_unlock(&ggggc_rootsLock);

        /* wait for the barrier once to allow collection */
        ggc_barrier_wait(&ggggc_worldBarrier);

        /* wait for the barrier to know when collection is done */
        ggc_barrier_wait(&ggggc_worldBarrier);

        /* now we can reset our pool */
        ggggc_pool0 = ggggc_gen0;
    }
}