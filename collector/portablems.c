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
    void *ret = NULL;
    struct FreeListNode **freeListCur;
    int retried = 0;

    /* look for space on the free list */
retry:
    for (freeListCur = &freeList;
         *freeListCur;
         freeListCur = &(*freeListCur)->next) {

        if ((*freeListCur)->size == size) {
            /* exactly the right size */
            ret = (void *) *freeListCur;
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
            ret = (void *) *freeListCur;
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

static struct ToSearch toSearchList;

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
    }
}

/* sweep this heap */
static void sweep(struct GGGGC_Pool *pool)
{
    ggc_size_t survivors = 0, total = 0;
    struct FreeListNode *lastFree = NULL;

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

    lastFree->next = NULL;

    /* fix up the free list */
    for (lastFree = freeList; lastFree; lastFree = lastFree->next) {
        if (lastFree->zero) {
            lastFree->size = lastFree->zero;
            lastFree->zero = 0;
        } else {
            abort();
        }
    }

    /* possibly expand our heap */
    if (survivors > total>>1) {
        /* less than half free */
        total = survivors - (total>>1);
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
    struct GGGGC_PoolList pool0Node, *plCur;
    struct GGGGC_Pool *poolCur;
    struct GGGGC_PointerStackList pointerStackNode, *pslCur;
    struct GGGGC_PointerStack *psCur;
    struct ToSearch *toSearch;
    unsigned char genCur;
    ggc_size_t i;

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
    pool0Node.pool = ggggc_gen0;
    pool0Node.next = ggggc_blockedThreadPool0s;
    ggggc_rootPool0List = &pool0Node;
    pointerStackNode.pointerStack = ggggc_pointerStack;
    pointerStackNode.next = ggggc_blockedThreadPointerStacks;
    ggggc_rootPointerStackList = &pointerStackNode;
    ggc_mutex_unlock(&ggggc_rootsLock);

    /* stop the world */
    ggggc_stopTheWorld = 1;
    ggc_barrier_wait_raw(&ggggc_worldBarrier);
    ggggc_stopTheWorld = 0;

    /* wait for them to fill roots */
    ggc_barrier_wait_raw(&ggggc_worldBarrier);

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

    /* indicate that we're now ready to sweep */
    ggc_barrier_wait_raw(&ggggc_worldBarrier);

    /* sweep our own heap */
    sweep(ggggc_gen0);

    /* free the other threads */
    ggc_barrier_wait_raw(&ggggc_worldBarrier);
    ggc_mutex_unlock(&ggggc_worldBarrierLock);
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
    }

    return 0;
}

#endif
