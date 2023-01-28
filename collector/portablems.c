/*
 * Highly portable, simple mark and sweep collector
 *
 * Copyright (c) 2022, 2023 Gregor Richards
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

#ifdef GGGGC_COLLECTOR_PORTABLEMS

#include <stdio.h>
#include <string.h>

#include "ggggc-internals.h"

/* Free list. Free list elements are stored as [0, size in words, next]. */
struct FreeListNode {
    ggc_size_t zero, size;
    struct FreeListNode *next;
};
static ggc_thread_local struct FreeListNode *freeList = NULL;

/* create a pool and add it to the free list */
static struct GGGGC_Pool *ggggc_newPoolFree(int mustSucceed)
{
    struct GGGGC_Pool *pool;
    struct FreeListNode *node;

    /* allocate the pool */
    pool = ggggc_newPool(mustSucceed);
    if (!pool) return NULL;

    /* make it into a free list node */
    node = (struct FreeListNode *) (pool->start);
    node->zero = 0;
    node->size = pool->end - pool->start;
    node->next = freeList;
    freeList = node;

    return pool;
}

/* allocate an object */
void *ggggc_mallocRaw(struct GGGGC_Descriptor **descriptor, ggc_size_t size)
{
    struct GGGGC_Header *ret = NULL;
    struct FreeListNode **freeListCur;
    int retried = 0;

    /* look for space on the free list */
retry:
    for (freeListCur = &freeList;
         *freeListCur;
         freeListCur = &(*freeListCur)->next) {

        if ((*freeListCur)->size == size) {
            /* exactly the right size */
            ret = (struct GGGGC_Header *) (void *) *freeListCur;
            *freeListCur = (*freeListCur)->next;
            break;

        } else if ((*freeListCur)->size >= size + 4) {
            /* enough space to split */
            struct FreeListNode *next =
                (struct FreeListNode *) (void *)
                ((ggc_size_t *) *freeListCur + size);
            /* it's very important to do this backwards, in case they overlap */
            next->next = (*freeListCur)->next;
            next->size = (*freeListCur)->size - size;
            next->zero = 0;
            ret = (struct GGGGC_Header *) (void *) *freeListCur;
            *freeListCur = next;
            break;

        }
    }

    if (!ret) {
        /* Not enough space. Collect and maybe allocate more. */
        if (!ggggc_gen0) {
            /* We don't have any space at all! */
            ggggc_gen0 = ggggc_pool0 = ggggc_newPoolFree(1);
            goto retry;
        }

        /* Collect and retry. */
        if (retried < 1) {
            GGC_PUSH_1(*descriptor);
            retried = 1;
            ggggc_collect0(0);
            GGC_POP();
            goto retry;
        }

        /* Not enough space even after collection, so just make more. FIXME:
         * What if it's too big for a pool? */
        if (retried < 2) {
            struct GGGGC_Pool *pool;
            retried = 2;
            pool = ggggc_newPoolFree(0);
            if (pool) {
                pool->next = ggggc_gen0;
                ggggc_gen0 = pool;
            }
            goto retry;
        }

        /* Just not enough space! */
        fprintf(stderr, "GC: Out of memory!\n");
        abort();
    }

    memset(ret, 0, size * sizeof(ggc_size_t));

#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
    /* set its canary */
    ret->ggggc_memoryCorruptionCheck = GGGGC_MEMORY_CORRUPTION_VAL;
#endif

    return ret;
}

/* allocate an object */
void *ggggc_malloc(struct GGGGC_Descriptor *descriptor)
{
    struct GGGGC_Header *ret = (struct GGGGC_Header *)
        ggggc_mallocRaw(&descriptor, descriptor->size);
    ret->descriptor__ptr = descriptor;
    return ret;
}

/* generalized stop-the-world */
static void stopTheWorld(
    struct GGGGC_PoolList *pool0Node,
    struct GGGGC_PointerStackList *pointerStackNode
) {
    /* first, make sure we stop the world */
    while (ggc_mutex_trylock(&ggggc_worldBarrierLock) != 0) {
        /* somebody else is collecting */
        GGC_YIELD();
    }

    /* if nobody ever initialized the barrier, do so */
    if (ggggc_threadCount == (ggc_size_t) -1) {
        ggggc_threadCount = 1;
        ggc_barrier_init(&ggggc_worldBarrier, ggggc_threadCount);
    }

    /* initialize our roots */
    ggc_mutex_lock_raw(&ggggc_rootsLock);
    pool0Node->pool = ggggc_gen0;
    pool0Node->next = ggggc_blockedThreadPool0s;
    ggggc_rootPool0List = pool0Node;
    pointerStackNode->pointerStack = ggggc_pointerStack;
    pointerStackNode->next = ggggc_blockedThreadPointerStacks;
    ggggc_rootPointerStackList = pointerStackNode;
    ggc_mutex_unlock(&ggggc_rootsLock);

    /* stop the world */
    ggggc_stopTheWorld = 1;
    ggc_barrier_wait_raw(&ggggc_worldBarrier);
    ggggc_stopTheWorld = 0;

    /* wait for them to fill roots */
    ggc_barrier_wait_raw(&ggggc_worldBarrier);
}

/* generalized restart-the-world */
static void restartTheWorld()
{
    /* free the other threads */
    ggc_barrier_wait_raw(&ggggc_worldBarrier);
    ggc_mutex_unlock(&ggggc_worldBarrierLock);
}

/* getting the pool from an object is only needed by finalizers */
#ifdef GGGGC_FEATURE_FINALIZERS
struct GGGGC_Pool *ggggc_poolOf(void *obj)
{
    struct GGGGC_PoolList *plCur, pool0Node;
    struct GGGGC_PointerStackList pointerStackNode;

    /* hopefully it's a local pool, so we can simply find it */
    struct GGGGC_Pool *pool;
    for (pool = ggggc_gen0; pool; pool = pool->next) {
        if (obj >= (void *) pool->start && obj < (void *) pool->end)
            return pool;
    }

    /* Not a local pool. We have to stop the world to scan all the other
     * threads' pools, and in portablems, the only way to do that is a complete
     * collection. */
    stopTheWorld(&pool0Node, &pointerStackNode);

    for (plCur = ggggc_rootPool0List; plCur; plCur = plCur->next) {
        for (pool = plCur->pool; pool; pool = pool->next) {
            if (obj >= (void *) pool->start && obj < (void *) pool->end)
                goto done;
        }
    }

    /* didn't find it! */
    abort();

done:
    /* we have to finish collection */
    ggggc_collect0(1);

    return pool;
}
#endif

static struct ToSearch toSearchList;

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
                if (nobj && nobj->ggggc_memoryCorruptionCheck != GGGGC_MEMORY_CORRUPTION_VAL) {
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

static void memoryCorruptionCheckPool(const char *when, struct GGGGC_Pool *pool) {
    union {
        ggc_size_t *w;
        struct GGGGC_Header *obj;
        struct FreeListNode *fln;
    } cur;

    cur.w = (ggc_size_t *) pool->start;
    while (cur.w < (ggc_size_t *) pool->end) {
        if (cur.fln->zero) {
            /* it's an allocated object */
            memoryCorruptionCheckObj(when, cur.obj);
            cur.w += cur.obj->descriptor__ptr->size;
        } else {
            /* it's a freelist entry */
            cur.w += cur.fln->size;
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
            memoryCorruptionCheckPool(when, poolCur);
        }
    }

    for (genCur = 1; genCur < GGGGC_GENERATIONS; genCur++) {
        for (poolCur = ggggc_gens[genCur]; poolCur; poolCur = poolCur->next) {
            memoryCorruptionCheckPool(when, poolCur);
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

/* mark this object and all objects it refers to */
static void mark(struct GGGGC_Header *obj)
{
    struct ToSearch *toSearch;
    struct GGGGC_Descriptor *descriptor;
    ggc_size_t descriptorI;
    void **objVp;
    ggc_size_t curWord;
#ifndef GGGGC_FEATURE_EXTTAG
    ggc_size_t curDescription, curDescriptorWord;
#endif

    TOSEARCH_INIT();
    TOSEARCH_ADD(obj);

    while (toSearch->used) {
        TOSEARCH_POP(struct GGGGC_Header *, obj);

#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
        /* check for pre-corruption */
        if (obj->ggggc_memoryCorruptionCheck != GGGGC_MEMORY_CORRUPTION_VAL) {
            fprintf(stderr, "GGGGC: Memory corruption (precheck)!\n");
            abort();
        }
#endif

        /* check if it's already marked */
        descriptor = obj->descriptor__ptr;
        descriptorI = (ggc_size_t) (void *) descriptor;
        if (descriptorI & 1) {
            /* already marked */
            continue;
        }

        /* mark it */
        descriptorI |= 1;
        obj->descriptor__ptr = (struct GGGGC_Descriptor *) (void *) descriptorI;

        /* mark its descriptor */
        TOSEARCH_ADD(descriptor);

        /* and recurse */
#ifndef GGGGC_FEATURE_EXTTAG
        if (descriptor->pointers[0] & 1)
#else
        if (descriptor->tags[0] != 1)
#endif
        {
            /* it has pointers */
            objVp = (void **) obj;
#ifndef GGGGC_FEATURE_EXTTAG
            curDescriptorWord = 0;
            curDescription = descriptor->pointers[0] >> 1;
#endif

            for (curWord = 1; curWord < descriptor->size; curWord++) {
#ifndef GGGGC_FEATURE_EXTTAG
                if (curWord % GGGGC_BITS_PER_WORD == 0)
                    curDescription = descriptor->pointers[++curDescriptorWord];
                if (curDescription & 1)
#else
                if ((descriptor->tags[curWord] & 1) == 0)
#endif
                {
                    /* it's a pointer */
                    if (objVp[curWord]) {
                        TOSEARCH_ADD(objVp[curWord]);
                    }
                }
#ifndef GGGGC_FEATURE_EXTTAG
                curDescription >>= 1;
#endif
            }
        }

#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
        /* check for post-corruption */
        if (obj->ggggc_memoryCorruptionCheck != GGGGC_MEMORY_CORRUPTION_VAL) {
            fprintf(stderr, "GGGGC: Memory corruption (postcheck)!\n");
            abort();
        }
#endif
    }
}

/* used by both sweep and clearFreeList */
static ggc_thread_local ggc_size_t survivors, total;

/* sweep this heap */
static void sweep(struct GGGGC_Pool *pool)
{
    struct FreeListNode *lastFree = NULL;

    survivors = total = 0;
    freeList = NULL;

    for (; pool; pool = pool->next) {
        union {
            ggc_size_t *w;
            struct GGGGC_Header *obj;
            struct FreeListNode *fl;
        } cur;
        ggc_size_t *end;

        pool->survivors = 0;

        /* start at the beginning of the pool */
        cur.w = pool->start;

        /* and work our way to the end */
        end = pool->end;
        while (cur.w < end) {
            struct GGGGC_Descriptor *descriptor;
            struct FreeListNode *nextFree;
            ggc_size_t descriptorI;

            /* it's either a freelist entry already or an allocated object */
            descriptorI = (ggc_size_t) (void *) cur.obj->descriptor__ptr;
            if (descriptorI & 1) {
                /* it's marked, so still in use */
                descriptorI--;
                descriptor = (struct GGGGC_Descriptor *) (void *) descriptorI;
                pool->survivors += descriptor->size;
                cur.obj->descriptor__ptr = descriptor;
                cur.w += descriptor->size;
                continue;
            }

            /* it's free */
            /* the size is in the same location as a descriptor's size, so we
             * use the zero space for the size temporarily, then put the size in
             * the correct place in a later pass. */
            if (cur.fl->zero == 0) {
                /* it was already free */
                cur.fl->zero = cur.fl->size;
            } else {
                /* it was allocated */
                descriptor = (struct GGGGC_Descriptor *) (void *) descriptorI;
                cur.fl->zero = descriptor->size;
            }

            /* free this object */
            if (lastFree) {
                if (cur.w == ((ggc_size_t *) lastFree) + lastFree->zero) {
                    /* coalesce (remember, size is currently in the zero slot) */
                    lastFree->zero += cur.fl->zero;
                } else {
                    /* free separately */
                    lastFree->next = cur.fl;
                    lastFree = cur.fl;
                }
            } else {
                freeList = lastFree = cur.fl;
            }

            /* and continue */
            cur.w = ((ggc_size_t *) lastFree) + lastFree->zero;
        }

        /* now count */
        survivors += pool->survivors;
        total += pool->end - pool->start;
    }

    if (lastFree)
        lastFree->next = NULL;
}

/* clear the free list after sweeping */
void clearFreeList()
{
    struct GGGGC_Pool *pool;
    struct FreeListNode *fln;
    for (fln = freeList; fln; fln = fln->next) {
        fln->size = fln->zero;
        fln->zero = 0;
    }

    /* possibly expand our heap */
    if (survivors > total>>1) {
        /* less than half free */
        total = survivors - (total - survivors) + GGGGC_POOL_BYTES;
        while (total > GGGGC_POOL_BYTES) {
            pool = ggggc_newPoolFree(0);
            if (!pool)
                break;
            pool->next = ggggc_gen0;
            ggggc_gen0 = pool;
            total -= GGGGC_POOL_BYTES;
        }
    }
}

/* run garbage collection */
void ggggc_collect0(unsigned char gen)
{
    struct GGGGC_PoolList pool0Node;
    struct GGGGC_PointerStackList pointerStackNode, *pslCur;
    struct GGGGC_PointerStack *psCur;
    ggc_size_t i;

#ifdef GGGGC_FEATURE_FINALIZERS
    struct GGGGC_PoolList *plCur;
    GGGGC_FinalizerEntry readyFinalizers = NULL;
#endif

    /* gen is used to indicate "we already stopped the world" */
    if (!gen)
        stopTheWorld(&pool0Node, &pointerStackNode);

#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
    memoryCorruptionCheck("pre-collection");
#endif

    /* mark from roots */
    for (pslCur = ggggc_rootPointerStackList; pslCur; pslCur = pslCur->next) {
        for (psCur = pslCur->pointerStack; psCur; psCur = psCur->next) {
            for (i = 0; i < psCur->size; i++) {
                struct GGGGC_Header **h = (struct GGGGC_Header **)
                    psCur->pointers[i];
                if (h && *h)
                    mark(*h);
            }
        }
    }

#ifdef GGGGC_FEATURE_FINALIZERS
    /* look for finalized objects */
    for (plCur = ggggc_rootPool0List; plCur; plCur = plCur->next) {
        struct GGGGC_Pool *pool;
        for (pool = plCur->pool; pool; pool = pool->next) {
            GGGGC_FinalizerEntry survivingFinalizers = NULL, finalizer,
                nextFinalizer;
            finalizer = (GGGGC_FinalizerEntry) pool->finalizers;
            for (; finalizer; finalizer = nextFinalizer) {
                struct GGGGC_Header *obj =
                    (struct GGGGC_Header *) finalizer->obj__ptr;
                nextFinalizer = finalizer->next__ptr;
                if (((ggc_size_t) (void *) obj->descriptor__ptr) & 1) {
                    /* It's marked. This object survives. */
                    finalizer->next__ptr = survivingFinalizers;
                    survivingFinalizers = finalizer;

                } else {
                    /* Unmarked. Finalize this. */
                    finalizer->next__ptr = readyFinalizers;
                    readyFinalizers = finalizer;

                }
            }
            pool->finalizers = (void *) survivingFinalizers;
        }
    }

    /* now mark all finalizers, so they aren't swept before being finalized */
    for (plCur = ggggc_rootPool0List; plCur; plCur = plCur->next) {
        struct GGGGC_Pool *pool;
        for (pool = plCur->pool; pool; pool = pool->next) {
            if (pool->finalizers)
                mark(pool->finalizers);
        }
    }
    if (readyFinalizers)
        mark(&readyFinalizers->header);
#endif

    /* indicate that we're now ready to sweep */
    ggc_barrier_wait_raw(&ggggc_worldBarrier);

    /* sweep our own heap */
    sweep(ggggc_gen0);

#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
    {
        struct FreeListNode *f;
        for (f = freeList; f; f = f->next)
            memset(f + 1, 0, f->size * sizeof(ggc_size_t) - sizeof(struct FreeListNode));
    }
    memoryCorruptionCheck("post-collection");
#endif

    restartTheWorld();

    /* clear our freelist */
    clearFreeList();

#ifdef GGGGC_FEATURE_FINALIZERS
    /* run any finalizers */
    if (readyFinalizers)
        ggggc_runFinalizers(readyFinalizers);
#endif
}

/* run full garbage collection (in gembc, just collect0) */
void ggggc_collect()
{
    ggggc_collect0(0);
}

/* explicitly yield to the collector */
int ggggc_yield()
{
    struct GGGGC_PoolList pool0Node;
    struct GGGGC_PointerStackList pointerStackNode;

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
        ggc_mutex_unlock(&ggggc_rootsLock);

        /* wait for the barrier once to allow marking */
        ggc_barrier_wait_raw(&ggggc_worldBarrier);

        /* wait until marking is done */
        ggc_barrier_wait_raw(&ggggc_worldBarrier);

        /* local sweep */
        sweep(ggggc_gen0);

        /* and continue */
        ggc_barrier_wait_raw(&ggggc_worldBarrier);

        /* we can clear our freelist independently */
        clearFreeList();
    }

    return 0;
}

#endif
