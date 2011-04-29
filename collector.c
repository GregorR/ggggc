/*
 * The collector proper
 *
 * Copyright (C) 2010 Gregor Richards
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "ggggc.h"
#include "ggggc_internal.h"
#include "buffer.h"

BUFFER(voidpp, void **);

/* one global tocheck, so we don't have to re-init and re-free it */
static struct Buffer_voidpp tocheck;

/* place for each thread to leave its individual pointer stacks */
static struct Buffer_voidpp *chcheck;

/* number of threads, and ID of maximum thread */
static unsigned int threadCount, maxThread;
static GGC_th_barrier_t threadBarrier;
static GGC_th_rwlock_t threadLock;

void GGGGC_collector_init()
{
    size_t sz = GGGGC_PSTACK_SIZE;
    GGC_TLS_INIT(ggggc_pstack);
    GGC_TLS_SET(ggggc_pstack, (struct GGGGC_PStack *) malloc(sizeof(struct GGGGC_PStack) - sizeof(void *) + sz * sizeof(void *)));
    GGC_TLS_GET(struct GGGGC_PStack *, ggggc_pstack)->rem = sz;
    GGC_TLS_GET(struct GGGGC_PStack *, ggggc_pstack)->cur = GGC_TLS_GET(struct GGGGC_PStack *, ggggc_pstack)->ptrs;
    INIT_BUFFER(tocheck);
    threadCount = 1;
    maxThread = 0;
    threadBarrier = GGC_alloc_barrier();
    GGC_barrier_init(threadBarrier, 1);
    threadLock = GGC_alloc_rwlock();
    GGC_rwlock_init(threadLock);
}

/* expand the root stack to support N more variables */
void GGGGC_pstackExpand(size_t by)
{
    size_t cur = (GGC_TLS_GET(struct GGGGC_PStack *, ggggc_pstack)->cur - GGC_TLS_GET(struct GGGGC_PStack *, ggggc_pstack)->ptrs);
    size_t sz = cur + GGC_TLS_GET(struct GGGGC_PStack *, ggggc_pstack)->rem;
    size_t newsz = sz;
    while (newsz < cur + by)
        newsz *= 2;
    GGC_TLS_SET(ggggc_pstack, (struct GGGGC_PStack *) realloc(GGC_TLS_GET(struct GGGGC_PStack *, ggggc_pstack), sizeof(struct GGGGC_PStack) - sizeof(void *) + newsz * sizeof(void *)));
    GGC_TLS_GET(struct GGGGC_PStack *, ggggc_pstack)->rem = newsz - cur;
    GGC_TLS_GET(struct GGGGC_PStack *, ggggc_pstack)->cur = GGC_TLS_GET(struct GGGGC_PStack *, ggggc_pstack)->ptrs + cur;
}

void GGGGC_collect(unsigned char gen)
{
    int i, j, c;
    size_t p, survivors, heapsz;
    struct GGGGC_Pool *gpool;
    unsigned char nextgen;
    int nislast;

    int serialThread;
    unsigned int tid, ti;
    struct Buffer_voidpp mycheck;

#define BARRIER do { \
    fprintf(stderr, "Thread %u waiting ...\n", tid); \
    serialThread = (GGC_barrier_wait(threadBarrier) == GGC_BARRIER_SERIAL_THREAD); \
    fprintf(stderr, "Thread %u done waiting.\n", tid); \
} while (0)

    /* get our thread ID */
    tid = (unsigned int) (size_t) GGC_TLS_GET(void *, GGC_thread_identifier);

    /* make sure all threads synchronize here */
    GGC_rwlock_rdlock(threadLock);
    BARRIER;

    /* first, create an array for all the pointer stacks */
    if (serialThread) {
        SF(chcheck, malloc, NULL, ((maxThread + 1) * sizeof(struct Buffer_voidpp)));
        memset(chcheck, 0, (maxThread + 1) * sizeof(struct Buffer_voidpp));
    }
    BARRIER;

    /* now each thread puts its own roots in */
    INIT_BUFFER(mycheck);
    p = GGC_TLS_GET(struct GGGGC_PStack *, ggggc_pstack)->cur - GGC_TLS_GET(struct GGGGC_PStack *, ggggc_pstack)->ptrs;

    while (BUFFER_SPACE(mycheck) < p) {
        EXPAND_BUFFER(mycheck);
    }

    for (i = 0; i < p; i++) {
        if (*GGC_TLS_GET(struct GGGGC_PStack *, ggggc_pstack)->ptrs[i])
            WRITE_ONE_BUFFER(mycheck, GGC_TLS_GET(struct GGGGC_PStack *, ggggc_pstack)->ptrs[i]);
    }
    chcheck[tid] = mycheck;
    BARRIER;

    /* only the serial thread will GC */
    if (!serialThread) {
        fprintf(stderr, "%u isn't serial.\n", tid);
        BARRIER;
        GGC_rwlock_rdunlock(threadLock);
        return;
    }
    sleep(1);

retry:
    survivors = heapsz = 0;
    nextgen = gen+1;
    nislast = 0;
    if (nextgen == GGGGC_GENERATIONS) {
        nislast = 1;
    }

    /* first add all the roots */
    tocheck.bufused = 0;
    for (ti = 0; ti <= maxThread; ti++) {
        if (chcheck[ti].buf) {
            WRITE_BUFFER(tocheck, chcheck[ti].buf, chcheck[ti].bufused);
        }
    }

    /* get all the remembered cards */
    for (i = gen + 1; i < GGGGC_GENERATIONS; i++) {
        gpool = ggggc_gens[i];

        for (; gpool; gpool = gpool->next) {
            for (c = 0; c < GGGGC_CARDS_PER_POOL; c++) {
                if (gpool->remember[c]) {
                    /* remembered, add the card */
                    size_t base = (size_t) (((char *) gpool) + (GGGGC_CARD_BYTES * c));
                    struct GGGGC_Header *first = (struct GGGGC_Header *) ((char *) base + gpool->firstobj[c]);
                    struct GGGGC_Header *obj = first;

                    /* walk through this card */
                    while (base == ((size_t) obj & GGGGC_NOCARD_MASK) && (char *) obj < gpool->top) {
                        void **ptr = (void **) (obj + 1);

                        /* add all its pointers */
                        for (j = 0; j < obj->ptrs; j++, ptr++) {
                            if (*ptr)
                                WRITE_ONE_BUFFER(tocheck, ptr);
                        }

                        obj = (struct GGGGC_Header *) ((char *) obj + obj->sz);
                    }

                }
            }
        }
    }

    /* now just iterate while we have things to check */
    for (i = 0; i < tocheck.bufused; i++) {
        void **ptoch = tocheck.buf[i];
        struct GGGGC_Header *objtoch = (struct GGGGC_Header *) *ptoch - 1;

#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
        /* Make sure it's valid */
        if (objtoch->magic != GGGGC_HEADER_MAGIC) {
            fprintf(stderr, "Memory corruption!\n");
            *((int *) 0) = 0;
        }
#endif

        /* OK, we have the object to check, has it already moved? */
        while (objtoch->sz & 1) {
            /* move it */
            objtoch = (struct GGGGC_Header *) (objtoch->sz & ((size_t) -1 << 1));
            *ptoch = (void *) (objtoch + 1);
        }

        /* Do we need to reclaim? */
        if (objtoch->gen <= gen) {
            void **ptr;

            /* nope, get a new one */
            struct GGGGC_Header *newobj =
                (struct GGGGC_Header *) GGGGC_trymalloc_gen(nextgen, nislast, objtoch->sz, objtoch->ptrs);
            if (newobj == NULL) {
                /* ACK! Out of memory! Need more GC! */
                gen++;
                if (gen >= GGGGC_GENERATIONS) {
                    fprintf(stderr, "Memory exhausted during GC???\n");
                    *((int *) 0) = 0;
                }
                goto retry;
            }
            newobj -= 1; /* get back to the header */

            /* copy it in */
            survivors += objtoch->sz;
            memcpy((void *) newobj, (void *) objtoch, objtoch->sz);
            newobj->gen = nextgen;
            objtoch->sz = ((size_t) newobj) | 1; /* forwarding pointer */

            /* and check its pointers */
            ptr = (void **) (newobj + 1);
            for (j = 0; j < newobj->ptrs; j++, ptr++) {
                if (*ptr)
                    WRITE_ONE_BUFFER(tocheck, ptr);
            }

            /* finally, update the pointer we're looking at */
            *ptoch = (void *) (newobj + 1);
        }
    }

    /* and clear the generations we've done */
    for (i = 0; i <= gen; i++) {
        gpool = ggggc_gens[i];
        for (; gpool; gpool = gpool->next) {
            heapsz += GGGGC_POOL_BYTES;
            GGGGC_clear_pool(gpool);
        }
    }

    /* clear the remember set of the next one */
    gpool = ggggc_gens[gen+1];
    for (; gpool; gpool = gpool->next) {
        memset(gpool->remember, 0, GGGGC_CARDS_PER_POOL);
    }

    /* and if we're doing the last (last+1 really) generation, treat it like two-space copying */
    if (nislast) {
        gpool = ggggc_gens[gen+1];
        ggggc_gens[gen+1] = ggggc_gens[gen];
        ggggc_gens[gen] = gpool;

        /* update the gen property */
        for (; gpool; gpool = gpool->next) {
            struct GGGGC_Header *upd =
                (struct GGGGC_Header *) (gpool->firstobj + GGGGC_CARDS_PER_POOL);
            while ((void *) upd < (void *) gpool->top) {
                upd->gen = GGGGC_GENERATIONS - 1;
                upd = (struct GGGGC_Header *) ((char *) upd + upd->sz);
            }
        }
    }

    /* and finally, heuristically allocate more space */
    if (survivors > heapsz / 2) {
        for (i = 0; i <= GGGGC_GENERATIONS; i++) {
            gpool = ggggc_gens[i];
            for (; gpool->next; gpool = gpool->next);
            gpool->next = GGGGC_alloc_pool();
        }
        gpool = ggggc_gens[0];
        for (; gpool->next; gpool = gpool->next);
        ggggc_heurpool = gpool;
    }
    ggggc_allocpool = ggggc_gens[0];

    /* clear up all the space used by the other threads */
    for (ti = 0; ti <= maxThread; ti++) {
        if (chcheck[ti].buf) {
            FREE_BUFFER(chcheck[ti]);
        }
    }
    free(chcheck);
    BARRIER;
    GGC_rwlock_rdunlock(threadLock);
}

/* keep track of the number of threads we have */
void GGGGC_new_thread()
{
    unsigned int tid = (unsigned int) (size_t) GGC_TLS_GET(void *, GGC_thread_identifier);
    size_t sz = GGGGC_PSTACK_SIZE;

    /* set up this thread's pointer stack */
    GGC_TLS_SET(ggggc_pstack, (struct GGGGC_PStack *) malloc(sizeof(struct GGGGC_PStack) - sizeof(void *) + sz * sizeof(void *)));
    GGC_TLS_GET(struct GGGGC_PStack *, ggggc_pstack)->rem = sz;
    GGC_TLS_GET(struct GGGGC_PStack *, ggggc_pstack)->cur = GGC_TLS_GET(struct GGGGC_PStack *, ggggc_pstack)->ptrs;

    GGC_rwlock_wrlock(threadLock);

    /* now mark this thread as existing and reset the barrier */
    threadCount++;
    if (tid > maxThread) maxThread = tid;
    GGC_barrier_destroy(threadBarrier);
    GGC_barrier_init(threadBarrier, threadCount);

    GGC_rwlock_wrunlock(threadLock);
}

void GGGGC_end_thread()
{
    GGC_rwlock_wrlock(threadLock);

    threadCount--;
    GGC_barrier_destroy(threadBarrier);
    GGC_barrier_init(threadBarrier, threadCount);

    GGC_rwlock_wrunlock(threadLock);

    /* free this thread's pointer stack */
    free(GGC_TLS_GET(struct GGGGC_PStack *, ggggc_pstack));
}
