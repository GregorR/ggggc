/*
 * Object allocation and the actual garbage collector
 *
 * Copyright (c) 2014-2023 Gregor Richards
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

#include "ggggc/gc.h"

#ifdef GGGGC_COLLECTOR_GEMBC

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "ggggc-internals.h"

#ifdef __cplusplus
extern "C" {
#endif

/* create a GC pool */
struct GGGGC_Pool *ggggc_newPoolGen(unsigned char gen, int mustSucceed)
{
    struct GGGGC_Pool *ret = ggggc_newPool(mustSucceed);
    if (!ret) return ret;

#if GGGGC_GENERATIONS > 1
    ret->gen = gen;
    
    /* clear the remembered set */
    if (gen > 0)
        memset(ret->remember, 0, GGGGC_CARDS_PER_POOL);

    /* the first object in the first usable card */
    ret->firstObject[GGGGC_CARD_OF(ret->start)] =
        (((ggc_size_t) ret->start) & GGGGC_CARD_INNER_MASK) / sizeof(ggc_size_t);
#endif

    return ret;
}

/* create a GC pool based on a prototype */
static struct GGGGC_Pool *newPoolGenProto(struct GGGGC_Pool *proto) {
#if GGGGC_GENERATIONS > 1
    return ggggc_newPoolGen(proto->gen, 0);
#else
    return ggggc_newPoolGen(0, 0);
#endif
}

/* NOTE: there is code duplication between ggggc_malloc and ggggc_mallocGen1
 * because I can't trust a compiler to inline and optimize for the 0 case */

/* allocate an object in generation 0 */
void *ggggc_mallocRaw(struct GGGGC_Descriptor **descriptor, /* descriptor to protect, if applicable */
                      ggc_size_t size /* size of object to allocate */
                      ) {
    struct GGGGC_Pool *pool;
    struct GGGGC_Header *ret;

retry:
    /* get our allocation pool */
    if (ggggc_pool0) {
        pool = ggggc_pool0;
    } else {
        ggggc_gen0 = ggggc_pool0 = pool = ggggc_newPoolGen(0, 1);
    }

    /* do we have enough space? */
    if (pool->end - pool->free >= size) {
        /* good, allocate here */
        ret = (struct GGGGC_Header *) pool->free;
        pool->free += size;

        /* and clear the rest (necessary since this goes to the untrusted mutator) */
        memset(ret, 0, size * sizeof(ggc_size_t));

#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
        /* set its canary */
        ret->ggggc_memoryCorruptionCheck = GGGGC_MEMORY_CORRUPTION_VAL;
#endif

    } else if (pool->next) {
        ggggc_pool0 = pool = pool->next;
        goto retry;

    } else {
        /* need to collect, which means we need to actually be a GC-safe function */
        GGC_PUSH_1(*descriptor);
        ggggc_collect0(0);
        GGC_POP();
        pool = ggggc_pool0;
        goto retry;

    }

    return ret;
}

#if GGGGC_GENERATIONS > 1
/* allocate an object in the requested generation > 0 */
void *ggggc_mallocGen1(ggc_size_t size, /* size of object to allocate */
                       unsigned char gen /* generation to allocate in */
                       ) {
    struct GGGGC_Pool *pool;
    struct GGGGC_Header *ret;

retry:
    /* get our allocation pool */
    if (ggggc_pools[gen]) {
        pool = ggggc_pools[gen];
    } else {
        ggggc_gens[gen] = ggggc_pools[gen] = pool = ggggc_newPoolGen(gen, 1);
    }

    /* do we have enough space? */
    if (pool->end - pool->free >= size) {
        ggc_size_t retCard, freeCard;

        /* good, allocate here */
        ret = (struct GGGGC_Header *) pool->free;
        pool->free += size;
        retCard = GGGGC_CARD_OF(ret);
        freeCard = GGGGC_CARD_OF(pool->free);

        /* if we passed a card, mark the first object */
        if (retCard != freeCard && pool->free < pool->end)
            pool->firstObject[freeCard] =
                ((ggc_size_t) pool->free & GGGGC_CARD_INNER_MASK) / sizeof(ggc_size_t);

        /* and clear the next descriptor so that it's clear there's no object
         * there (yet) */
        if (pool->free < pool->end) *pool->free = 0;

    } else if (pool->next) {
        ggggc_pools[gen] = pool = pool->next;
        goto retry;

    } else {
        /* failed to allocate */
        return NULL;

    }

    return ret;
}
#endif

/* allocate an object */
void *ggggc_malloc(struct GGGGC_Descriptor *descriptor)
{
    struct GGGGC_Header *ret = (struct GGGGC_Header *) ggggc_mallocRaw(&descriptor, descriptor->size);
    ret->descriptor__ptr = descriptor;
    return ret;
}


/* full collection */
#ifdef GGGGC_FEATURE_FINALIZERS
#define COLLECT_FULL_ARGS \
    GGGGC_FinalizerEntry *survivingFinalizersOut, \
    GGGGC_FinalizerEntry *survivingFinalizersTailOut, \
    GGGGC_FinalizerEntry *readyFinalizersOut
#else
#define COLLECT_FULL_ARGS void
#endif

void ggggc_collectFull(COLLECT_FULL_ARGS);

/* helper functions for full collection */
void ggggc_countUsed(struct GGGGC_Pool *);
void ggggc_compact(struct GGGGC_Pool *);
void ggggc_postCompact(struct GGGGC_Pool *);

/* follow a forwarding pointer to the object it actually represents */
#define IS_FORWARDED_OBJECT(obj) (((ggc_size_t) (obj)->descriptor__ptr) & 1)
#define FOLLOW_FORWARDED_OBJECT(obj) do { \
    while (IS_FORWARDED_OBJECT(obj)) \
        obj = (struct GGGGC_Header *) (((ggc_size_t) (obj)->descriptor__ptr) & (ggc_size_t) ~1); \
} while(0)

/* same for descriptors as a special case of objects */
#define IS_FORWARDED_DESCRIPTOR(d) IS_FORWARDED_OBJECT(&(d)->header)
#define FOLLOW_FORWARDED_DESCRIPTOR(d) do { \
    struct GGGGC_Header *dobj = &(d)->header; \
    FOLLOW_FORWARDED_OBJECT(dobj); \
    d = (struct GGGGC_Descriptor *) dobj; \
} while(0)

/* check if a value is user-tagged as a non-GC pointer. It's done here, while
 * adding to the to-check list, instead of while removing from the to-check
 * list, because the descriptor may be tagged by the GC, and we can only
 * distinguish the descriptor from other pointers at this point. */
#ifdef GGGGC_FEATURE_TAGGING
#define IS_TAGGED(p) ((ggc_size_t) (p) & (sizeof(ggc_size_t)-1))
#else
#define IS_TAGGED(p) 0
#endif

/* macro to add an object's pointers to the tosearch list */
#ifndef GGGGC_FEATURE_EXTTAG
#define ADD_OBJECT_POINTERS(obj, descriptor) do { \
    void **objVp = (void **) (obj); \
    ggc_size_t curWord, curDescription, curDescriptorWord = 0; \
    if (descriptor->pointers[0] & 1) { \
        /* it has pointers */ \
        curDescription = descriptor->pointers[0] >> 1; \
        for (curWord = 1; curWord < descriptor->size; curWord++) { \
            if (curWord % GGGGC_BITS_PER_WORD == 0) \
                curDescription = descriptor->pointers[++curDescriptorWord]; \
            if (curDescription & 1) \
                /* it's a pointer */ \
                if (objVp[curWord] && !IS_TAGGED(objVp[curWord])) \
                    TOSEARCH_ADD(&objVp[curWord]); \
            curDescription >>= 1; \
        } \
    } \
    TOSEARCH_ADD(&objVp[0]); \
} while(0)

#else /* !GGGGC_FEATURE_EXTTAG */
#define ADD_OBJECT_POINTERS(obj, descriptor) do { \
    void **objVp = (void **) (obj); \
    ggc_size_t curWord; \
    if (descriptor->tags[0] != 1) { \
        /* it has pointers */ \
        for (curWord = 1; curWord < descriptor->size; curWord++) { \
            if ((descriptor->tags[curWord] & 1) == 0) \
                /* it's a pointer */ \
                TOSEARCH_ADD(&objVp[curWord]); \
        } \
    } \
    TOSEARCH_ADD(&objVp[0]); \
} while (0)

#endif /* GGGGC_FEATURE_EXTTAG */

static struct ToSearch toSearchList;

#ifdef GGGGC_FEATURE_FINALIZERS
/* macro to handle the finalizers for a given pool */
#define FINALIZER_POOL() do { \
    finalizer = (GGGGC_FinalizerEntry) poolCur->finalizers; \
    poolCur->finalizers = NULL; \
    while (finalizer) { \
        obj = (struct GGGGC_Header *) finalizer; \
        if (IS_FORWARDED_OBJECT(obj)) \
            FOLLOW_FORWARDED_OBJECT(obj); \
        finalizer = (GGGGC_FinalizerEntry) obj; \
        nextFinalizer = finalizer->next__ptr; \
        \
        /* grab the object */ \
        obj = (struct GGGGC_Header *) finalizer->obj__ptr; \
        if (IS_FORWARDED_OBJECT(obj)) { \
            /* it survived, keep the finalizer */ \
            finalizer->next__ptr = survivingFinalizers; \
            survivingFinalizers = finalizer; \
            if (!survivingFinalizersTail) \
                survivingFinalizersTail = finalizer; \
            \
        } else { \
            /* object died, add this to ready finalizers */ \
            finalizer->next__ptr = readyFinalizers; \
            readyFinalizers = finalizer; \
            \
        } \
        \
        finalizer = nextFinalizer; \
    } \
} while(0)

/* preserve the surviving finalizers */
#define PRESERVE_FINALIZERS() do { \
    if (survivingFinalizers) { \
        poolCur = GGGGC_POOL_OF(survivingFinalizers); \
        survivingFinalizersTail->next__ptr = (GGGGC_FinalizerEntry) poolCur->finalizers; \
        poolCur->finalizers = survivingFinalizers; \
    } \
} while (0)

/* preserve ALL finalizers */
#define PRESERVE_ALL_FINALIZERS() do { \
    PRESERVE_FINALIZERS(); \
    survivingFinalizers = readyFinalizers; \
    PRESERVE_FINALIZERS(); \
} while(0)
#endif /* GGGGC_FEATURE_FINALIZERS */

#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
static void memoryCorruptionCheckObj(const char *when, struct GGGGC_Header *obj)
{
    struct GGGGC_Descriptor *descriptor = obj->descriptor__ptr;
    void **objVp = (void **) (obj);
    ggc_size_t curWord;
#ifndef GGGGC_FEATURE_EXTTAG
    ggc_size_t curDescription = 0, curDescriptorWord = 0;
#endif
    if (obj->ggggc_memoryCorruptionCheck != GGGGC_MEMORY_CORRUPTION_VAL) {
        fprintf(stderr, "GGGGC: Memory corruption (%s)!\n", when);
        abort();
    }
#ifndef GGGGC_FEATURE_EXTTAG
    if (descriptor->pointers[0] & 1)
#else
    if (descriptor->tags[0] != 1)
#endif
    {
        /* it has pointers */
        for (curWord = 0; curWord < descriptor->size; curWord++) {
#ifndef GGGGC_FEATURE_EXTTAG
            if (curWord % GGGGC_BITS_PER_WORD == 0)
                curDescription = descriptor->pointers[curDescriptorWord++];
            if (curDescription & 1)
#else
            if ((descriptor->tags[curWord] & 1) == 0)
#endif
            {
                /* it's a pointer */
                struct GGGGC_Header *nobj = (struct GGGGC_Header *) objVp[curWord];
                if (nobj && !IS_TAGGED(nobj) &&
                    nobj->ggggc_memoryCorruptionCheck != GGGGC_MEMORY_CORRUPTION_VAL) {
                    fprintf(stderr, "GGGGC: Memory corruption (%s nested)!\n", when);
                    abort();
                }
            }
#ifndef GGGGC_FEATURE_EXTTAG
            curDescription >>= 1;
#endif
        }
    } else {
        /* no pointers other than the descriptor */
        struct GGGGC_Header *nobj = (struct GGGGC_Header *) objVp[0];
        if (nobj && nobj->ggggc_memoryCorruptionCheck != GGGGC_MEMORY_CORRUPTION_VAL) {
            fprintf(stderr, "GGGGC: Memory corruption (%s nested)!\n", when);
            abort();
        }
    }
}

static void memoryCorruptionCheck(const char *when)
{
    struct GGGGC_PoolList *plCur;
    struct GGGGC_Pool *poolCur;
    struct GGGGC_PointerStackList *pslCur;
    struct GGGGC_PointerStack *psCur;
    unsigned char genCur;

    for (plCur = ggggc_rootPool0List; plCur; plCur = plCur->next) {
        for (poolCur = plCur->pool; poolCur; poolCur = poolCur->next) {
            struct GGGGC_Header *obj = (struct GGGGC_Header *) poolCur->start;
            for (; obj < (struct GGGGC_Header *) poolCur->free;
                 obj = (struct GGGGC_Header *) (((ggc_size_t) obj) + obj->descriptor__ptr->size * sizeof(ggc_size_t))) {
                memoryCorruptionCheckObj(when, obj);
            }
        }
    }

    for (genCur = 1; genCur < GGGGC_GENERATIONS; genCur++) {
        for (poolCur = ggggc_gens[genCur]; poolCur; poolCur = poolCur->next) {
            struct GGGGC_Header *obj = (struct GGGGC_Header *) poolCur->start;
            for (; obj < (struct GGGGC_Header *) poolCur->free;
                 obj = (struct GGGGC_Header *) (((ggc_size_t) obj) + obj->descriptor__ptr->size * sizeof(ggc_size_t))) {
                memoryCorruptionCheckObj(when, obj);
            }
        }
    }

    for (pslCur = ggggc_rootPointerStackList; pslCur; pslCur = pslCur->next) {
        for (psCur = pslCur->pointerStack; psCur; psCur = psCur->next) {
            ggc_size_t i;
            for (i = 0; i < psCur->size; i++) {
                struct GGGGC_Header *obj = *((struct GGGGC_Header **) psCur->pointers[i]);
                if (obj && !IS_TAGGED(obj))
                    memoryCorruptionCheckObj(when, obj);
            }
        }
    }
}
#endif

#ifdef GGGGC_DEBUG_REPORT_COLLECTIONS
static void report(unsigned char gen, const char *when)
{
    struct GGGGC_PoolList *plCur;
    struct GGGGC_Pool *poolCur;
    unsigned char genCur;
    ggc_size_t sz, used;

    fprintf(stderr, "Generation %d collection %s statistics\n", (int) gen, when);

    sz = used = 0;
    for (plCur = ggggc_rootPool0List; plCur; plCur = plCur->next) {
        for (poolCur = plCur->pool; poolCur; poolCur = poolCur->next) {
            sz += poolCur->end - poolCur->start;
            used += poolCur->free - poolCur->start;
        }
    }
    fprintf(stderr, " 0: %d/%d\n", (int) used, (int) sz);

    for (genCur = 1; genCur < GGGGC_GENERATIONS; genCur++) {
        sz = used = 0;
        for (poolCur = ggggc_gens[genCur]; poolCur; poolCur = poolCur->next) {
            sz += poolCur->end - poolCur->start;
            used += poolCur->free - poolCur->start;
        }
        fprintf(stderr, " %d: %d/%d\n", (int) genCur, (int) used, (int) sz);
    }
}
#endif

/* run a generation 0 collection */
void ggggc_collect0(unsigned char gen)
{
    struct GGGGC_PoolList pool0Node, *plCur;
    struct GGGGC_Pool *poolCur;
    struct GGGGC_PointerStackList pointerStackNode, *pslCur;
    struct GGGGC_PointerStack *psCur;
    struct ToSearch *toSearch;
    unsigned char genCur;
    ggc_size_t i;
#ifdef GGGGC_FEATURE_JITPSTACK
    struct GGGGC_JITPointerStackList jitPointerStackNode, *jpslCur;
    void **jpsCur;
#endif
#ifdef GGGGC_FEATURE_FINALIZERS
    int finalizersChecked;
    GGGGC_FinalizerEntry survivingFinalizers, survivingFinalizersTail, readyFinalizers;
    survivingFinalizers = survivingFinalizersTail = readyFinalizers = NULL;
#endif

    /* first, make sure we stop the world */
    while (ggc_mutex_trylock(&ggggc_worldBarrierLock) != 0) {
        /* somebody else is collecting */
        GGC_YIELD();
    }

    TOSEARCH_INIT();

    /* if nobody ever initialized the barrier, do so */
    if (ggggc_threadCount == (ggc_size_t) -1) {
        ggggc_threadCount = 1;
        ggc_barrier_init(&ggggc_worldBarrier, ggggc_threadCount);
    }

    /* initialize our roots */
    ggc_mutex_lock_raw(&ggggc_rootsLock);
    pool0Node.pool = ggggc_gen0;
    pool0Node.next = ggggc_blockedThreadPool0s;
    ggggc_rootPool0List = &pool0Node;
    pointerStackNode.pointerStack = ggggc_pointerStack;
    pointerStackNode.next = ggggc_blockedThreadPointerStacks;
    ggggc_rootPointerStackList = &pointerStackNode;

#ifdef GGGGC_FEATURE_JITPSTACK
    jitPointerStackNode.cur = ggc_jitPointerStack;
    jitPointerStackNode.top = ggc_jitPointerStackEnd;
    jitPointerStackNode.next = ggggc_blockedThreadJITPointerStacks;
    ggggc_rootJITPointerStackList = &jitPointerStackNode;
#endif

    ggc_mutex_unlock(&ggggc_rootsLock);

    /* stop the world */
    ggggc_stopTheWorld = 1;
    ggc_barrier_wait_raw(&ggggc_worldBarrier);
    ggggc_stopTheWorld = 0;

    /* wait for them to fill roots */
    ggc_barrier_wait_raw(&ggggc_worldBarrier);

#ifdef GGGGC_DEBUG_REPORT_COLLECTIONS
    report(gen, "pre-collection");
#endif

#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
    memoryCorruptionCheck("pre-collection");
#endif

    /************************************************************
     * COLLECTION
     ***********************************************************/
collect:

    /* possibly jump to a full collection */
    if (
#if GGGGC_GENERATIONS > 1
        gen >= GGGGC_GENERATIONS - 1
#else
        1
#endif
    ) {
        ggggc_collectFull(
#ifdef GGGGC_FEATURE_FINALIZERS
            &survivingFinalizers, &survivingFinalizersTail, &readyFinalizers
#endif
        );
        goto postCollect;
    }

#if GGGGC_GENERATIONS > 1
    /* add our roots to the to-search list */
    for (pslCur = ggggc_rootPointerStackList; pslCur; pslCur = pslCur->next) {
        for (psCur = pslCur->pointerStack; psCur; psCur = psCur->next) {
            for (i = 0; i < psCur->size; i++) {
                if (psCur->pointers[i] && !IS_TAGGED(*(void **) psCur->pointers[i]))
                    TOSEARCH_ADD(psCur->pointers[i]);
            }
        }
    }

#ifdef GGGGC_FEATURE_JITPSTACK
    for (jpslCur = ggggc_rootJITPointerStackList; jpslCur; jpslCur = jpslCur->next) {
        for (jpsCur = jpslCur->cur; jpsCur < jpslCur->top; jpsCur++) {
#ifndef GGGGC_FEATURE_EXTTAG
            if (!IS_TAGGED(*((void *) jpsCur)))
                TOSEARCH_ADD(jpsCur);
#else
            int wordIdx;
            size_t tags = *((ggc_size_t *) jpsCur);
            for (wordIdx = 0; wordIdx < sizeof(ggc_size_t); wordIdx++) {
                unsigned char tag = tags & 0xFF;
                tags >>= 8;
                if (tag == 0xFF) {
                    /* End-of-tags tag */
                    break;
                }
                jpsCur++;
                /* Lowest bit indicates pointer */
                if ((tag & 0x1) == 0 && *jpsCur)
                    TOSEARCH_ADD(jpsCur);
            }
#endif /* GGGGC_FEATURE_EXTTAG */
        }
    }
#endif /* GGGGC_FEATURE_JITPSTACK */

    /* add our remembered sets to the to-search list */
    for (genCur = gen + 1; genCur < GGGGC_GENERATIONS; genCur++) {
        for (poolCur = ggggc_gens[genCur]; poolCur; poolCur = poolCur->next) {
            for (i = 0; i < GGGGC_CARDS_PER_POOL; i++) {
                if (poolCur->remember[i]) {
                    struct GGGGC_Header *obj = (struct GGGGC_Header *)
                        ((ggc_size_t) poolCur + i * GGGGC_CARD_BYTES + poolCur->firstObject[i] * sizeof(ggc_size_t));
                    while (GGGGC_CARD_OF(obj) == i) {
                        ADD_OBJECT_POINTERS(obj, obj->descriptor__ptr);
                        obj = (struct GGGGC_Header *)
                            ((ggc_size_t) obj + obj->descriptor__ptr->size * sizeof(ggc_size_t));
                        if (obj->descriptor__ptr == NULL) break;
                    }
                }
            }
        }
    }

#ifdef GGGGC_FEATURE_FINALIZERS
    /* start pre-finalizers */
    finalizersChecked = 0;
#endif

    /* now test all our pointers */
    while (toSearch->used) {
        void **ptr;
        struct GGGGC_Header *obj;

        TOSEARCH_POP(void **, ptr);
        obj = (struct GGGGC_Header *) *ptr;
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
            nobj = (struct GGGGC_Header *) ggggc_mallocGen1(descriptor->size, gen + 1);
            if (!nobj) {
#ifdef GGGGC_FEATURE_FINALIZERS
                PRESERVE_ALL_FINALIZERS();
                survivingFinalizers = survivingFinalizersTail = readyFinalizers = NULL;
#endif

                /* failed to allocate, need to collect gen+1 too */
                gen += 1;
                TOSEARCH_INIT();
#ifdef GGGGC_DEBUG_REPORT_COLLECTIONS
                report(gen, "promotion");
#endif
                goto collect;
            }

            /* copy to the new object */
            memcpy(nobj, obj, descriptor->size * sizeof(ggc_size_t));

            /* mark it as forwarded */
            obj->descriptor__ptr = (struct GGGGC_Descriptor *) (((ggc_size_t) nobj) | 1);
            *ptr = obj = nobj;

            /* and add its pointers */
            ADD_OBJECT_POINTERS(obj, obj->descriptor__ptr);
        }

#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
        /* check for post-corruption */
        if (obj->ggggc_memoryCorruptionCheck != GGGGC_MEMORY_CORRUPTION_VAL) {
            fprintf(stderr, "GGGGC: Memory corruption (postcheck)!\n");
            abort();
        }
#endif

#ifdef GGGGC_FEATURE_FINALIZERS
        /* perhaps check finalizers */
        if (!toSearch->used && !finalizersChecked) {
            GGGGC_FinalizerEntry finalizer = NULL, nextFinalizer = NULL;

            finalizersChecked = 1;

            /* add all the finalizers themselves */
            for (plCur = ggggc_rootPool0List; plCur; plCur = plCur->next) {
                for (poolCur = plCur->pool; poolCur; poolCur = poolCur->next) {
                    FINALIZER_POOL();
                }
            }
            for (genCur = 1; genCur <= gen; genCur++) {
                for (poolCur = ggggc_gens[genCur]; poolCur; poolCur = poolCur->next) {
                    FINALIZER_POOL();
                }
            }

            /* then make sure the finalizer queues get promoted */
            TOSEARCH_ADD(&survivingFinalizers);
            TOSEARCH_ADD(&survivingFinalizersTail);
            TOSEARCH_ADD(&readyFinalizers);
        }
#endif /* GGGGC_FEATURE_FINALIZERS */
    }
#endif /* GGGGC_GENERATIONS > 1 */

postCollect:

#ifdef GGGGC_FEATURE_FINALIZERS
    PRESERVE_FINALIZERS();
#endif

    /* heuristically expand too-small generations */
    for (plCur = ggggc_rootPool0List; plCur; plCur = plCur->next)
        ggggc_expandPoolList(plCur->pool, newPoolGenProto, 1);
    for (genCur = 1; genCur <= gen; genCur++)
        ggggc_expandPoolList(ggggc_gens[genCur], newPoolGenProto, 1);

    /* clear out the now-empty generations, unless we did a full collection */
    if (gen < GGGGC_GENERATIONS - 1) {
        for (plCur = ggggc_rootPool0List; plCur; plCur = plCur->next) {
            for (poolCur = plCur->pool; poolCur; poolCur = poolCur->next) {
                poolCur->free = poolCur->start;
            }
        }
        ggggc_pool0 = ggggc_gen0;
#if GGGGC_GENERATIONS > 1
        for (genCur = 1; genCur <= gen; genCur++) {
            for (poolCur = ggggc_gens[genCur]; poolCur; poolCur = poolCur->next) {
                memset(poolCur->remember, 0, GGGGC_CARDS_PER_POOL);
                poolCur->free = poolCur->start;
            }
            ggggc_pools[genCur] = ggggc_gens[genCur];
        }

        /* and the remembered sets */
        for (poolCur = ggggc_gens[gen+1]; poolCur; poolCur = poolCur->next) {
            memset(poolCur->remember, 0, GGGGC_CARDS_PER_POOL);
        }
#endif
    }

#ifdef GGGGC_DEBUG_REPORT_COLLECTIONS
    report(gen, "post-collection");
#endif

#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
    for (plCur = ggggc_rootPool0List; plCur; plCur = plCur->next) {
        for (poolCur = plCur->pool; poolCur; poolCur = poolCur->next) {
            memset(poolCur->free, 0, (poolCur->end - poolCur->free) * sizeof(ggc_size_t));
        }
    }
    for (genCur = 1; genCur < GGGGC_GENERATIONS; genCur++) {
        for (poolCur = ggggc_gens[genCur]; poolCur; poolCur = poolCur->next) {
            memset(poolCur->free, 0, (poolCur->end - poolCur->free) * sizeof(ggc_size_t));
        }
    }
    memoryCorruptionCheck("post-collection");
#endif

    /* free the other threads */
    ggc_barrier_wait_raw(&ggggc_worldBarrier);
    ggc_mutex_unlock(&ggggc_worldBarrierLock);

#ifdef GGGGC_FEATURE_FINALIZERS
    /* run our finalizers */
    if (readyFinalizers) ggggc_runFinalizers(readyFinalizers);
#endif
}

/* type for an element of our break table */
struct BreakTableEl {
    ggc_size_t *orig, *newL;
};

/* mark an object */
#define MARK(obj) do { \
    struct GGGGC_Header *hobj = (obj); \
    hobj->descriptor__ptr = (struct GGGGC_Descriptor *) \
        ((ggc_size_t) hobj->descriptor__ptr | 2); \
} while (0)

/* unmark a pointer */
#define UNMARK_PTR(type, ptr) \
    ((type *) ((ggc_size_t) (ptr) & (ggc_size_t) ~2))

/* get an object's descriptor, through markers */
#define MARKED_DESCRIPTOR(obj) \
    (UNMARK_PTR(struct GGGGC_Descriptor, (obj)->descriptor__ptr))

/* unmark an object */
#define UNMARK(obj) do { \
    struct GGGGC_Header *hobj = (obj); \
    hobj->descriptor__ptr = UNMARK_PTR(struct GGGGC_Descriptor, hobj->descriptor__ptr); \
} while (0)

/* is this pointer marked? */
#define IS_MARKED_PTR(ptr) ((ggc_size_t) (ptr) & 2)

/* is this object marked? */
#define IS_MARKED(obj) IS_MARKED_PTR((obj)->descriptor__ptr)

/* find the new location of an object that's been compacted */
#define FOLLOW_COMPACTED_OBJECT(obj) do { \
    ggc_size_t *dobj = (ggc_size_t *) (obj); \
    struct GGGGC_Pool *cpool = GGGGC_POOL_OF(dobj); \
    struct BreakTableEl *bel = findBreakTableEntry( \
        (struct BreakTableEl *) cpool->breakTable, cpool->breakTableSize, \
        dobj); \
    if (bel) { \
        dobj -= bel->orig - bel->newL; \
        obj = dobj; \
    } \
} while(0)

/* special case for compacted descriptors */
#define FOLLOW_COMPACTED_DESCRIPTOR(d) do { \
    ggc_size_t *dcobj = (ggc_size_t *) (d); \
    FOLLOW_COMPACTED_OBJECT(dcobj); \
    d = (struct GGGGC_Descriptor *) dcobj; \
} while(0)

/* comparator for break table elements */
static int breakTableComparator(const void *lv, const void *rv)
{
    struct BreakTableEl *l, *r;
    l = (struct BreakTableEl *) lv;
    r = (struct BreakTableEl *) rv;
    if (l->orig < r->orig) {
        return -1;
    } else if (l->orig > r->orig) {
        return 1;
    }
    return 0;
}

/* find the break table entry that matches a given pointer */
static struct BreakTableEl *findBreakTableEntry(struct BreakTableEl *breakTable, ggc_size_t breakTableSize, ggc_size_t *loc)
{
    ggc_size_t cur = breakTableSize / 2;
    ggc_size_t step = cur / 2;

    if (breakTableSize == 0) return NULL;
    if (step == 0) step = 1;

    while (1) {
        if (breakTable[cur].orig <= loc) {
            /* this might include the location being searched */
            if (cur == breakTableSize - 1 ||
                breakTable[cur+1].orig > loc)
                return &breakTable[cur];

            /* otherwise, we're too low */
            cur += step;
            if (cur >= breakTableSize) cur = breakTableSize - 1;
            step /= 2;
            if (step == 0) step = 1;

        } else if (breakTable[cur].orig > loc) {
            /* we're too high */
            if (cur == 0) return NULL;
            if (step > cur)
                cur = 0;
            else
                cur -= step;
            step /= 2;
            if (step == 0) step = 1;

        }
    }
}

/* perform a full, in-place collection */
void ggggc_collectFull(COLLECT_FULL_ARGS)
{
    struct GGGGC_PoolList *plCur;
    struct GGGGC_Pool *poolCur;
    struct GGGGC_PointerStackList *pslCur;
    struct GGGGC_PointerStack *psCur;
    struct ToSearch *toSearch;
    unsigned char genCur;
    ggc_size_t i;
#ifdef GGGGC_FEATURE_JITPSTACK
    struct GGGGC_JITPointerStackList *jpslCur;
    void **jpsCur;
#endif
#ifdef GGGGC_FEATURE_FINALIZERS
    int finalizersChecked = 0;
    GGGGC_FinalizerEntry survivingFinalizers, survivingFinalizersTail, readyFinalizers;
    survivingFinalizers = survivingFinalizersTail = readyFinalizers = NULL;
#endif

    TOSEARCH_INIT();

    /* add our roots to the to-search list */
    for (pslCur = ggggc_rootPointerStackList; pslCur; pslCur = pslCur->next) {
        for (psCur = pslCur->pointerStack; psCur; psCur = psCur->next) {
            for (i = 0; i < psCur->size; i++) {
                if (psCur->pointers[i] && !IS_TAGGED(*(void **) psCur->pointers[i]))
                    TOSEARCH_ADD(psCur->pointers[i]);
            }
        }
    }

#ifdef GGGGC_FEATURE_JITPSTACK
    for (jpslCur = ggggc_rootJITPointerStackList; jpslCur; jpslCur = jpslCur->next) {
        for (jpsCur = jpslCur->cur; jpsCur < jpslCur->top; jpsCur++) {
#ifndef GGGGC_FEATURE_EXTTAG
            if (!IS_TAGGED(*(void **) jpsCur))
                TOSEARCH_ADD(jpsCur);
#else
            int wordIdx;
            size_t tags = *((ggc_size_t *) jpsCur);
            for (wordIdx = 0; wordIdx < sizeof(ggc_size_t); wordIdx++) {
                unsigned char tag = tags & 0xFF;
                tags >>= 8;
                if (tag == 0xFF) {
                    /* End-of-tags tag */
                    break;
                }
                jpsCur++;
                /* Lowest bit indicates pointer */
                if ((tag & 0x1) == 0 && *jpsCur)
                    TOSEARCH_ADD(jpsCur);
            }
#endif /* GGGGC_FEATURE_EXTTAG */
        }
    }
#endif /* GGGGC_FEATURE_JITPSTACK */

    /* now mark */
    while (toSearch->used) {
        void **ptr;
        struct GGGGC_Header *obj;
        ggc_size_t lastMark;

        TOSEARCH_POP(void **, ptr);
        obj = (struct GGGGC_Header *) *ptr;
        if (obj == NULL) continue;

        lastMark = IS_MARKED_PTR(obj);
        obj = UNMARK_PTR(struct GGGGC_Header, obj);

        /* if the object has moved */
        if (IS_FORWARDED_OBJECT(obj)) {
            /* then follow it */
            FOLLOW_FORWARDED_OBJECT(obj);
            *ptr = (void *) ((ggc_size_t) obj | lastMark);
        }

        /* if the object isn't already marked... */
        if (!IS_MARKED(obj)) {
            struct GGGGC_Descriptor *descriptor = obj->descriptor__ptr;

            /* then mark it */
            MARK(obj);
            GGGGC_POOL_OF(obj)->survivors += descriptor->size;

            /* add its pointers */
            ADD_OBJECT_POINTERS(obj, descriptor);
        }

#ifdef GGGGC_FEATURE_FINALIZERS
        /* possibly start handling finalizers */
        if (!toSearch->used && !finalizersChecked) {
            GGGGC_FinalizerEntry finalizer = NULL, nextFinalizer = NULL;

            finalizersChecked = 1;

            /* add all the finalizers themselves */
            for (plCur = ggggc_rootPool0List; plCur; plCur = plCur->next) {
                for (poolCur = plCur->pool; poolCur; poolCur = poolCur->next) {
                    FINALIZER_POOL();
                }
            }
            for (genCur = 1; genCur < GGGGC_GENERATIONS; genCur++) {
                for (poolCur = ggggc_gens[genCur]; poolCur; poolCur = poolCur->next) {
                    FINALIZER_POOL();
                }
            }

            /* then make sure the finalizer queues get promoted */
            TOSEARCH_ADD(&survivingFinalizers);
            TOSEARCH_ADD(&survivingFinalizersTail);
            TOSEARCH_ADD(&readyFinalizers);
        }
#endif /* GGGGC_FEATURE_FINALIZERS */
    }

    /* find all our sizes, for later compaction */
    for (plCur = ggggc_rootPool0List; plCur; plCur = plCur->next) {
        for (poolCur = plCur->pool; poolCur; poolCur = poolCur->next) {
            ggggc_countUsed(poolCur);
        }
    }
    for (genCur = 1; genCur < GGGGC_GENERATIONS; genCur++) {
        for (poolCur = ggggc_gens[genCur]; poolCur; poolCur = poolCur->next) {
            ggggc_countUsed(poolCur);
        }
    }
    
    /* perform compaction */
    for (plCur = ggggc_rootPool0List; plCur; plCur = plCur->next) {
        for (poolCur = plCur->pool; poolCur; poolCur = poolCur->next) {
            ggggc_compact(poolCur);
        }
    }
    for (genCur = 1; genCur < GGGGC_GENERATIONS; genCur++) {
        for (poolCur = ggggc_gens[genCur]; poolCur; poolCur = poolCur->next) {
            ggggc_compact(poolCur);
        }
    }

    /* then update our pointers */
    for (pslCur = ggggc_rootPointerStackList; pslCur; pslCur = pslCur->next) {
        for (psCur = pslCur->pointerStack; psCur; psCur = psCur->next) {
            ggc_size_t ***pointers = (ggc_size_t ***) psCur->pointers;
            for (i = 0; i < psCur->size; i++) {
                if (*pointers[i] && !IS_TAGGED(*pointers[i]))
                    FOLLOW_COMPACTED_OBJECT(*pointers[i]);
            }
        }
    }

#ifdef GGGGC_FEATURE_JITPSTACK
    for (jpslCur = ggggc_rootJITPointerStackList; jpslCur; jpslCur = jpslCur->next) {
        for (jpsCur = jpslCur->cur; jpsCur < jpslCur->top; jpsCur++) {
#ifndef GGGGC_FEATURE_EXTTAG
            if (*jpsCur)
                FOLLOW_COMPACTED_OBJECT(*jpsCur);
#else
            int wordIdx;
            size_t tags = *((ggc_size_t *) jpsCur);
            for (wordIdx = 0; wordIdx < sizeof(ggc_size_t); wordIdx++) {
                unsigned char tag = tags & 0xFF;
                tags >>= 8;
                if (tag == 0xFF) {
                    /* End-of-tags tag */
                    break;
                }
                jpsCur++;
                /* Lowest bit indicates pointer */
                if ((tag & 0x1) == 0 && *jpsCur)
                    FOLLOW_COMPACTED_OBJECT(*jpsCur);
            }
#endif /* GGGGC_FEATURE_EXTTAG */
        }
    }
#endif /* GGGGC_FEATURE_JITPSTACK */

#ifdef GGGGC_FEATURE_FINALIZERS
#define F(finalizer) do { \
    if (finalizer) { \
        void *ptr = (void *) (finalizer); \
        FOLLOW_COMPACTED_OBJECT(ptr); \
        (finalizer) = (GGGGC_FinalizerEntry) ptr; \
    } \
} while(0)
    F(survivingFinalizers);
    F(survivingFinalizersTail);
    F(readyFinalizers);
#undef F
#endif

    for (plCur = ggggc_rootPool0List; plCur; plCur = plCur->next) {
        for (poolCur = plCur->pool; poolCur; poolCur = poolCur->next) {
            ggggc_postCompact(poolCur);
        }
    }

    for (genCur = 1; genCur < GGGGC_GENERATIONS; genCur++) {
        for (poolCur = ggggc_gens[genCur]; poolCur; poolCur = poolCur->next) {
            ggggc_postCompact(poolCur);
        }
    }

    /* reset the pools */
    ggggc_pool0 = ggggc_gen0;
    for (genCur = 1; genCur < GGGGC_GENERATIONS; genCur++) {
        for (poolCur = ggggc_gens[genCur]; poolCur; poolCur = poolCur->next) {
            if (poolCur->free < poolCur->end) *poolCur->free = 0;
        }
        ggggc_pools[genCur] = ggggc_gens[genCur];
    }

#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
    for (plCur = ggggc_rootPool0List; plCur; plCur = plCur->next) {
        for (poolCur = plCur->pool; poolCur; poolCur = poolCur->next) {
            memset(poolCur->free, 0, (poolCur->end - poolCur->free) * sizeof(ggc_size_t));
        }
    }
    for (genCur = 1; genCur < GGGGC_GENERATIONS; genCur++) {
        for (poolCur = ggggc_gens[genCur]; poolCur; poolCur = poolCur->next) {
            memset(poolCur->free, 0, (poolCur->end - poolCur->free) * sizeof(ggc_size_t));
        }
    }
#endif

#ifdef GGGGC_FEATURE_FINALIZERS
    *survivingFinalizersOut = survivingFinalizers;
    *survivingFinalizersTailOut = survivingFinalizersTail;
    *readyFinalizersOut = readyFinalizers;
#endif
}

/* determine the size of every contiguous chunk of used or unused space in this
 * pool, and leave it visible in the pool:
 *
 * * The size of the first allocated chunk (at pool->start) is placed in
 *   pool->breakTableSize
 * * The size of each unused chunk is stored at the first word of that chunk 
 * * The size of each used chunk is stored before the first word of that chunk
 *   (i.e., in the unused space)
 */
void ggggc_countUsed(struct GGGGC_Pool *pool)
{
    ggc_size_t *cur, *next;

    /* first figure out the size of the first chunk of memory */
    for (cur = pool->start; 
         cur < pool->free && IS_MARKED((struct GGGGC_Header *) cur);
         cur += ((struct GGGGC_Header *) cur)->descriptor__ptr->size) {
        UNMARK((struct GGGGC_Header *) cur);
    }
    pool->breakTableSize = cur - pool->start;
    if (cur >= pool->end) return;

    while (1) {
        /* we are currently in an UNUSED chunk. Find out how big it is */
        for (next = cur;
             next < pool->free && !IS_MARKED((struct GGGGC_Header *) next);) {
            struct GGGGC_Header *obj = (struct GGGGC_Header *) next;

            /* if it's unmarked, it may even have moved */
            if (IS_FORWARDED_OBJECT(obj)) {
                /* we'll need to find its descriptor */
                FOLLOW_FORWARDED_OBJECT(obj);
                /* we don't need to follow the descriptor if it moved, as the
                 * old data is still intact */
            }

            next += MARKED_DESCRIPTOR(obj)->size;
        }
        if (next >= pool->free) next = pool->end;

        /* mark its size */
        *cur = next - cur;
        cur = next;
        if (next >= pool->end) break;

        /* we are currently in a USED chunk. Find out how big it is */
        for (next = cur;
             next < pool->free && IS_MARKED((struct GGGGC_Header *) next);
             next += ((struct GGGGC_Header *) next)->descriptor__ptr->size) {
            UNMARK((struct GGGGC_Header *) next);
        }

        /* mark its size */
        cur[-1] = next - cur;
        cur = next;
        if (next >= pool->end) break;
    }
}

/* compact a pool and create its break table */
void ggggc_compact(struct GGGGC_Pool *pool)
{
    ggc_size_t chSize, fchSize;
    struct BreakTableEl *bt = NULL, *btEnd;
    ggc_size_t i, j, *cur;

    /* ggggc_countUsed put the size of the first contiguous chunk in breakTableSize, so start from there */
    cur = pool->start + pool->breakTableSize;
    pool->free = cur;
    pool->breakTableSize = 0;
    pool->breakTable = NULL;
    if (cur >= pool->end) return; /* no compaction to be done! */

    bt = btEnd = (struct BreakTableEl *) cur;
    fchSize = *cur;

    while (1) {
        /* we are currently in an UNUSED chunk. Skip it */
        cur += fchSize;
        if (cur >= pool->end) break;

        /* we are currently in a USED chunk. Get the size of this chunk and the size of the following free chunk */
        chSize = cur[-1];
        if (cur + chSize < pool->end)
            fchSize = cur[chSize];
        else
            fchSize = 0;

        /* now copy in the data while rolling forward the bt */
        for (i = 0; i < chSize; i += sizeof(struct BreakTableEl) / sizeof(ggc_size_t)) {
            /* move the bt entry out of the way */
            *btEnd++ = *bt++;

            /* and copy in the data */
            for (j = 0; j < sizeof(struct BreakTableEl) / sizeof(ggc_size_t); j++)
                pool->free[i+j] = cur[i+j];
        }

        /* add the bt table entry */
        btEnd->orig = cur;
        btEnd->newL = pool->free;
        btEnd++;
        pool->free += chSize;
        cur += chSize;

#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
        if ((ggc_size_t *) bt < pool->free) abort();
        if ((ggc_size_t *) btEnd >= cur + fchSize) abort();
#endif

        if (cur >= pool->end) break;
    }

#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
    if ((ggc_size_t *) btEnd >= pool->end) abort();
#endif

    pool->breakTableSize = btEnd - bt;
    pool->breakTable = bt;

    /* then sort the break table */
    qsort(bt, pool->breakTableSize, sizeof(struct BreakTableEl), breakTableComparator);
}

/* reset all the pointers in a pool after compaction */
void ggggc_postCompact(struct GGGGC_Pool *pool)
{
    ggc_size_t **obj;
    ggc_size_t card = 0, lastCard = (ggc_size_t) -1;

#if GGGGC_GENERATIONS > 1
    /* this is going to fill in the remembered sets */
    if (pool->gen) memset(pool->remember, 0, GGGGC_CARDS_PER_POOL);
#endif

    for (obj = (ggc_size_t **) pool->start; obj < (ggc_size_t **) pool->free;) {
        struct GGGGC_Descriptor *descriptor;
        ggc_size_t curWord;
#ifndef GGGGC_FEATURE_EXTTAG
        ggc_size_t curDescription = 0, curDescriptorWord = 0;
#endif

#if GGGGC_GENERATIONS > 1
        /* set its card metadata */
        if (pool->gen) {
            card = GGGGC_CARD_OF(obj);
            if (card != lastCard) {
                pool->firstObject[card] = ((ggc_size_t) obj & GGGGC_CARD_INNER_MASK) / sizeof(ggc_size_t);
                lastCard = card;
            }
        }
#endif

        /* get the descriptor */
        descriptor = (struct GGGGC_Descriptor *) obj[0];
        FOLLOW_COMPACTED_DESCRIPTOR(descriptor);

        /* and walk through all its pointers */
#ifndef GGGGC_FEATURE_EXTTAG
        if (descriptor->pointers[0] & 1)
#else
        if (descriptor->tags[0] != 1)
#endif
        {
            /* it has pointers */
            for (curWord = 0; curWord < descriptor->size; curWord++) {
#ifndef GGGGC_FEATURE_EXTTAG
                if (curWord % GGGGC_BITS_PER_WORD == 0)
                    curDescription = descriptor->pointers[curDescriptorWord++];
                if ((curDescription & 1) && obj[curWord] &&
                    !IS_TAGGED(obj[curWord]))
#else
                if ((descriptor->tags[curWord] & 1) == 0 && obj[curWord])
#endif
                {
                    /* it's a pointer */
                    FOLLOW_COMPACTED_OBJECT(obj[curWord]);

#if GGGGC_GENERATIONS > 1
                    /* if it's a cross-generational pointer, remember it */
                    if (GGGGC_POOL_OF(obj[curWord])->gen < pool->gen)
                        pool->remember[card] = 1;
#endif
                }
#ifndef GGGGC_FEATURE_EXTTAG
                curDescription >>= 1;
#endif
            }
        } else {
            /* no pointers other than the descriptor */
            FOLLOW_COMPACTED_OBJECT(obj[0]);
#if GGGGC_GENERATIONS > 1
            if (GGGGC_POOL_OF(obj[0])->gen < pool->gen)
                pool->remember[card] = 1;
#endif
        }

        obj += descriptor->size;
    }

#if GGGGC_GENERATIONS > 1
    /* and perhaps set firstObject for the free space */
    if (pool->gen) {
        card = GGGGC_CARD_OF(pool->free);
        if (card != lastCard)
            pool->firstObject[card] = ((ggc_size_t) pool->free & GGGGC_CARD_INNER_MASK) / sizeof(ggc_size_t);
    }
#endif

#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
    for (obj = (ggc_size_t **) pool->start; obj < (ggc_size_t **) pool->free;) {
        struct GGGGC_Descriptor *descriptor = (struct GGGGC_Descriptor *) obj[0];
        if ((ggc_size_t) obj[1] != GGGGC_MEMORY_CORRUPTION_VAL) {
            fprintf(stderr, "GGGGC: Memory corruption (post-compaction)\n");
            abort();
        }
        if (IS_MARKED_PTR(descriptor->header.descriptor__ptr)) {
            fprintf(stderr, "GGGGC: Memory corruption (post-compaction surviving mark)\n");
            abort();
        }
        obj += descriptor->size;
    }
#endif
}

/* run a full collection of every generation */
void ggggc_collect()
{
    unsigned char gen;
    for (gen = 0; gen < GGGGC_GENERATIONS; gen++)
        ggggc_collect0(gen);
}

/* explicitly yield to the collector */
int ggggc_yield()
{
    struct GGGGC_PoolList pool0Node;
    struct GGGGC_PointerStackList pointerStackNode;
#ifdef GGGGC_FEATURE_JITPSTACK
    struct GGGGC_JITPointerStackList jitPointerStackNode;
#endif

    if (ggggc_stopTheWorld) {
        /* wait for the barrier once to stop the world */
        ggc_barrier_wait_raw(&ggggc_worldBarrier);

        /* feed it my globals */
        ggc_mutex_lock_raw(&ggggc_rootsLock);
        pool0Node.pool = ggggc_gen0;
        pool0Node.next = ggggc_rootPool0List;
        ggggc_rootPool0List = &pool0Node;
        pointerStackNode.pointerStack = ggggc_pointerStack;
        pointerStackNode.next = ggggc_rootPointerStackList;
        ggggc_rootPointerStackList = &pointerStackNode;

#ifdef GGGGC_FEATURE_JITPSTACK
        jitPointerStackNode.cur = ggc_jitPointerStack;
        jitPointerStackNode.top = ggc_jitPointerStackEnd;
        jitPointerStackNode.next = ggggc_rootJITPointerStackList;
        ggggc_rootJITPointerStackList = &jitPointerStackNode;
#endif

        ggc_mutex_unlock(&ggggc_rootsLock);

        /* wait for the barrier once to allow collection */
        ggc_barrier_wait_raw(&ggggc_worldBarrier);

        /* wait for the barrier to know when collection is done */
        ggc_barrier_wait_raw(&ggggc_worldBarrier);

        /* now we can reset our pool */
        ggggc_pool0 = ggggc_gen0;
    }

    return 0;
}

#ifdef __cplusplus
}
#endif

#endif
