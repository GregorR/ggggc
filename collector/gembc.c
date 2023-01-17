/*
 * Object allocation and the actual garbage collector
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

        ret->descriptor__ptr = NULL;
#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
        /* set its canary */
        ret->ggggc_memoryCorruptionCheck = GGGGC_MEMORY_CORRUPTION_VAL;
#endif

        /* and clear the rest (necessary since this goes to the untrusted mutator) */
        memset(ret + 1, 0, size * sizeof(ggc_size_t) - sizeof(struct GGGGC_Header));

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
void ggggc_collectFull(void);

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

/* macro to add an object's pointers to the tosearch list */
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
                TOSEARCH_ADD(&objVp[curWord]); \
            curDescription >>= 1; \
        } \
    } \
    TOSEARCH_ADD(&objVp[0]); \
} while(0)

static struct ToSearch toSearchList;

#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
static void memoryCorruptionCheckObj(const char *when, struct GGGGC_Header *obj)
{
    struct GGGGC_Descriptor *descriptor = obj->descriptor__ptr;
    void **objVp = (void **) (obj);
    ggc_size_t curWord, curDescription = 0, curDescriptorWord = 0;
    if (obj->ggggc_memoryCorruptionCheck != GGGGC_MEMORY_CORRUPTION_VAL) {
        fprintf(stderr, "GGGGC: Memory corruption (%s)!\n", when);
        abort();
    }
    if (descriptor->pointers[0] & 1) {
        /* it has pointers */
        for (curWord = 0; curWord < descriptor->size; curWord++) {
            if (curWord % GGGGC_BITS_PER_WORD == 0)
                curDescription = descriptor->pointers[curDescriptorWord++];
            if (curDescription & 1) {
                /* it's a pointer */
                struct GGGGC_Header *nobj = (struct GGGGC_Header *) objVp[curWord];
                if (nobj && nobj->ggggc_memoryCorruptionCheck != GGGGC_MEMORY_CORRUPTION_VAL) {
                    fprintf(stderr, "GGGGC: Memory corruption (%s nested)!\n", when);
                    abort();
                }
            }
            curDescription >>= 1;
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
                if (obj)
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
    struct GGGGC_JITPointerStackList jitPointerStackNode, *jpslCur;
    struct GGGGC_PointerStack *psCur;
    void **jpsCur;
    struct ToSearch *toSearch;
    unsigned char genCur;
    ggc_size_t i;

    /* first, make sure we stop the world */
    while (ggc_mutex_trylock(&ggggc_worldBarrierLock) != 0) {
        /* somebody else is collecting */
        while (!ggggc_stopTheWorld) {}
        GGC_YIELD();
        return;
    }

    TOSEARCH_INIT();

    /* if nobody ever initialized the barrier, do so */
    if (ggggc_threadCount == 0) {
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
    jitPointerStackNode.cur = ggc_jitPointerStack;
    jitPointerStackNode.top = ggc_jitPointerStackTop;
    jitPointerStackNode.next = ggggc_blockedThreadJITPointerStacks;
    ggggc_rootJITPointerStackList = &jitPointerStackNode;
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

#if GGGGC_GENERATIONS == 1
    /* with only one generation, we only want a full collection */
    ggggc_collectFull();

#else
    /************************************************************
     * COLLECTION
     ***********************************************************/
#if GGGGC_GENERATIONS > 2
collect:
#endif

    /* add our roots to the to-search list */
    for (pslCur = ggggc_rootPointerStackList; pslCur; pslCur = pslCur->next) {
        for (psCur = pslCur->pointerStack; psCur; psCur = psCur->next) {
            for (i = 0; i < psCur->size; i++) {
                TOSEARCH_ADD(psCur->pointers[i]);
            }
        }
    }
    for (jpslCur = ggggc_rootJITPointerStackList; jpslCur; jpslCur = jpslCur->next) {
        for (jpsCur = jpslCur->cur; jpsCur < jpslCur->top; jpsCur++) {
            TOSEARCH_ADD(jpsCur);
        }
    }

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
                /* failed to allocate, need to collect gen+1 too */
                gen += 1;
                TOSEARCH_INIT();
#ifdef GGGGC_DEBUG_REPORT_COLLECTIONS
                report(gen, "promotion");
#endif
#if GGGGC_GENERATIONS > 2
                if (gen >= GGGGC_GENERATIONS - 1) {
                    ggggc_collectFull();
                    break;
                } else goto collect;
#else
                ggggc_collectFull();
                break;
#endif
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
    }

#endif /* GGGGC_GENERATIONS > 1 */

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
void ggggc_collectFull()
{
    struct GGGGC_PoolList *plCur;
    struct GGGGC_Pool *poolCur;
    struct GGGGC_PointerStackList *pslCur;
    struct GGGGC_PointerStack *psCur;
    struct GGGGC_JITPointerStackList *jpslCur;
    void **jpsCur;
    struct ToSearch *toSearch;
    unsigned char genCur;
    ggc_size_t i;

    TOSEARCH_INIT();

    /* add our roots to the to-search list */
    for (pslCur = ggggc_rootPointerStackList; pslCur; pslCur = pslCur->next) {
        for (psCur = pslCur->pointerStack; psCur; psCur = psCur->next) {
            for (i = 0; i < psCur->size; i++) {
                TOSEARCH_ADD(psCur->pointers[i]);
            }
        }
    }
    for (jpslCur = ggggc_rootJITPointerStackList; jpslCur; jpslCur = jpslCur->next) {
        for (jpsCur = jpslCur->cur; jpsCur < jpslCur->top; jpsCur++) {
            TOSEARCH_ADD(jpsCur);
        }
    }

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
                if (*pointers[i])
                    FOLLOW_COMPACTED_OBJECT(*pointers[i]);
            }
        }
    }
    for (jpslCur = ggggc_rootJITPointerStackList; jpslCur; jpslCur = jpslCur->next) {
        for (jpsCur = jpslCur->cur; jpsCur < jpslCur->top; jpsCur++) {
            if (*jpsCur)
                FOLLOW_COMPACTED_OBJECT(*jpsCur);
        }
    }
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
        ggc_size_t curWord, curDescription = 0, curDescriptorWord = 0;

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
        if (descriptor->pointers[0] & 1) {
            /* it has pointers */
            for (curWord = 0; curWord < descriptor->size; curWord++) {
                if (curWord % GGGGC_BITS_PER_WORD == 0)
                    curDescription = descriptor->pointers[curDescriptorWord++];
                if ((curDescription & 1) && obj[curWord]) {
                    /* it's a pointer */
                    FOLLOW_COMPACTED_OBJECT(obj[curWord]);

#if GGGGC_GENERATIONS > 1
                    /* if it's a cross-generational pointer, remember it */
                    if (GGGGC_POOL_OF(obj[curWord])->gen < pool->gen)
                        pool->remember[card] = 1;
#endif
                }
                curDescription >>= 1;
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

/* explicitly yield to the collector */
int ggggc_yield()
{
    struct GGGGC_PoolList pool0Node;
    struct GGGGC_PointerStackList pointerStackNode;
    struct GGGGC_JITPointerStackList jitPointerStackNode;

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
        jitPointerStackNode.cur = ggc_jitPointerStack;
        jitPointerStackNode.top = ggc_jitPointerStackTop;
        jitPointerStackNode.next = ggggc_rootJITPointerStackList;
        ggggc_rootJITPointerStackList = &jitPointerStackNode;
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
