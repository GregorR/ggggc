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
#include <sys/resource.h>
#include <sys/time.h>

#include "ggggc.h"
#include "ggggc_internal.h"
#include "buffer.h"

BUFFER(voidpp, void **);

/* one global tocheck, so we don't have to re-init and re-free it */
static struct Buffer_voidpp tocheck;

void GGGGC_collector_init()
{
    ggggc_pstack = NULL;
    ggggc_dpstack = NULL;
    INIT_BUFFER(tocheck);
}


static __inline__ void scan(void **ptr, int ct)
{
    int i, skip = 0;
    for (i = 0; i < ct; i++, ptr++) {
        void *pval = *ptr;
        if (!skip) {
            if (GGC_UNTAG(pval))
                WRITE_ONE_BUFFER(tocheck, ptr);
            if ((size_t) pval & 0x1)
                skip = 1;
        } else {
            skip = 0;
        }
    }
}

void GGGGC_collect(unsigned char gen)
{
    int i, c;
    size_t survivors, heapsz;
    struct GGGGC_Pool *gpool;
    unsigned char nextgen;
    int nislast;
    struct GGGGC_PStack *p;
#ifdef RUSAGE_SELF
    long faultBegin, faultEnd;
    static int faultingCollections = 0;
#endif
#ifdef GGGGC_DEBUG_COLLECTION_TIME
    struct timeval tva, tvb;
#endif

#ifdef GGGGC_DEBUG_COLLECTION_TIME
    gettimeofday(&tva, NULL);
#endif

#ifdef RUSAGE_SELF
    {
        struct rusage ru;
        if (getrusage(RUSAGE_SELF, &ru) == 0) faultBegin = ru.ru_minflt;
        else faultBegin = 0;
    }
#endif

retry:
    survivors = heapsz = 0;
    nextgen = gen+1;
    nislast = 0;
    if (nextgen == GGGGC_GENERATIONS) {
        nislast = 1;
    }

    /* first add the roots */
    tocheck.bufused = 0;

    for (p = ggggc_pstack; p; p = p->next) {
        void ***ptrs = p->ptrs;
        for (i = 0; ptrs[i]; i++) {
            if (*ptrs[i])
                WRITE_ONE_BUFFER(tocheck, ptrs[i]);
        }
    }

    for (p = ggggc_dpstack; p; p = p->next) {
        void ***ptrs = p->ptrs;
        for (i = 0; ptrs[i]; i++) {
            if (GGC_UNTAG(*ptrs[i]))
                WRITE_ONE_BUFFER(tocheck, ptrs[i]);
            if (!GGC_TAGS(*ptrs[i]) && ptrs[i][1])
                WRITE_ONE_BUFFER(tocheck, ptrs[i] + 1);
        }
    }

    /* get the Fythe register bank */
    scan((void **) GGGGC_fytheConstBank, GGGGC_fytheConstBankPtrs);

    /* and the Fythe stack */
    scan((void **) GGGGC_fytheStackBase, (void **) GGGGC_fytheStackTop - (void **) GGGGC_fytheStackBase);

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
                        /* add all its pointers */
                        scan((void **) (obj + 1), obj->ptrs);

                        /* and step */
                        obj = (struct GGGGC_Header *) ((char *) obj + obj->sz);
                    }

                }
            }
        }
    }

    /* now just iterate while we have things to check */
    gpool = NULL;
    for (i = tocheck.bufused - 1; i >= 0; i = tocheck.bufused - 1) {
        void **ptoch = tocheck.buf[i];
        struct GGGGC_Header *objtoch = (struct GGGGC_Header *) GGC_UNTAG(*ptoch) - 1;
        tocheck.bufused--;

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
            *ptoch = (void *) (GGC_TAGS(*ptoch) | (size_t) (objtoch + 1));
        }

        /* Do we need to reclaim? */
        if (objtoch->gen <= gen) {
            /* nope, get a new one */
            struct GGGGC_Header *newobj =
                (struct GGGGC_Header *) GGGGC_trymalloc_gen(nextgen, nislast, &gpool, objtoch->sz, objtoch->ptrs);
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
            scan((void **) (newobj + 1), newobj->ptrs);

            /* finally, update the pointer we're looking at */
            *ptoch = (void *) (GGC_TAGS(*ptoch) | (size_t) (newobj + 1));
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

    /* and if we're doing the last (last+1 really) generation, treat it like two-space copying
     * NOTE: This step is expensive, but maintaining the same intelligence
     * during collection proper, particularly due to forwarding pointers from
     * both generations, seems to be more expensive */
    if (nislast) {
        gpool = ggggc_gens[gen+1];
        ggggc_gens[gen+1] = ggggc_gens[gen];
        ggggc_gens[gen] = gpool;

        /* update the gen property */
        for (; gpool; gpool = gpool->next) {
            struct GGGGC_Header *upd =
                (struct GGGGC_Header *) (gpool->firstobj + GGGGC_CARDS_PER_POOL);
            void *top = gpool->top;
            while ((void *) upd < top) {
                upd->gen = GGGGC_GENERATIONS - 1;
                upd = (struct GGGGC_Header *) ((char *) upd + upd->sz);
            }
        }
    }

    /* and finally, heuristically allocate more space or restrict */
#ifdef RUSAGE_SELF
    {
        struct rusage ru;
        if (getrusage(RUSAGE_SELF, &ru) == 0) faultEnd = ru.ru_minflt;
        else faultEnd = 0;
    }
    if (faultEnd > faultBegin) faultingCollections++;
    else faultingCollections = 0;
    if (faultingCollections == 0)
#else
    if (survivors > heapsz / 2)
#endif
    {
        for (i = 0; i <= GGGGC_GENERATIONS; i++) {
            gpool = ggggc_gens[i];
            for (; gpool->next; gpool = gpool->next);
            gpool->next = GGGGC_alloc_pool();
            if (i == 0) {
                ggggc_heurpool = gpool->next;
                ggggc_heurpoolmax = (char *) ggggc_heurpool + GGGGC_HEURISTIC_MAX;
            }
        }
    } else
#ifdef RUSAGE_SELF
    if (faultingCollections > 1)
#else
    if (0)
#endif
    {
        for (i = 0; i <= GGGGC_GENERATIONS; i++) {
            gpool = ggggc_gens[i];
            for (; gpool->next && gpool->next->next; gpool = gpool->next);
            if (gpool->next && (void *) gpool->next->top == (void *) (gpool + 1)) {
                GGGGC_free_pool(gpool->next);
                gpool->next = NULL;
            }
            if (i == 0) {
                ggggc_heurpool = gpool->next ? gpool->next : gpool;
                ggggc_heurpoolmax = (char *) ggggc_heurpool + GGGGC_HEURISTIC_MAX;
            }
        }
    }

    ggggc_allocpool = ggggc_gens[0];

#ifdef GGGGC_DEBUG_COLLECTION_TIME
    gettimeofday(&tvb, NULL);
    fprintf(stderr, "Generation %d collection finished in %dus\n",
            (int) gen,
            (tvb.tv_sec - tva.tv_sec) * 1000000 + (tvb.tv_usec - tva.tv_usec));
#endif
}
