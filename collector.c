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

#include "ggggc.h"
#include "ggggc_internal.h"
#include "buffer.h"

BUFFER(voidpp, void **);

/* one global tocheck, so we don't have to re-init and re-free it */
static struct Buffer_voidpp tocheck;

void GGGGC_collector_init()
{
    size_t sz = GGGGC_PSTACK_SIZE;
    ggggc_pstack = (struct GGGGC_PStack *) malloc(sizeof(struct GGGGC_PStack) - sizeof(void *) + sz * sizeof(void *));
    ggggc_pstack->cur = ggggc_pstack->ptrs;
    INIT_BUFFER(tocheck);
}

void GGGGC_collect(unsigned char gen)
{
    int i, j, c;
    size_t p, survivors, heapsz;
    struct GGGGC_Pool *gpool;
    unsigned char nextgen;
    int nislast;

retry:
    survivors = heapsz = 0;
    nextgen = gen+1;
    nislast = 0;
    if (nextgen == GGGGC_GENERATIONS) {
        nislast = 1;
    }

    /* first add the roots */
    tocheck.bufused = 0;
    p = ggggc_pstack->cur - ggggc_pstack->ptrs;

    while (BUFFER_SPACE(tocheck) < p) {
        EXPAND_BUFFER(tocheck);
    }

    for (i = 0; i < p; i++) {
        if (*ggggc_pstack->ptrs[i])
            WRITE_ONE_BUFFER(tocheck, ggggc_pstack->ptrs[i]);
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
    for (i = tocheck.bufused - 1; i >= 0; i = tocheck.bufused - 1) {
        void **ptoch = tocheck.buf[i];
        struct GGGGC_Header *objtoch = (struct GGGGC_Header *) *ptoch - 1;
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
            if (i == 0) {
                ggggc_heurpool = gpool->next;
                ggggc_heurpoolmax = (char *) ggggc_heurpool + GGGGC_HEURISTIC_MAX;
            }
        }
    }

    ggggc_allocpool = ggggc_gens[0];
}
