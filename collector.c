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

static struct Buffer_voidpp gcRoots;

void GGGGC_collector_init()
{
    INIT_BUFFER(gcRoots);
}

void GGGGC_push(void **ptr)
{
    WRITE_BUFFER(gcRoots, &ptr, 1);
}

void GGGGC_pop(int ct)
{
    gcRoots.bufused -= ct;
}

void GGGGC_collect(unsigned char gen)
{
    struct Buffer_voidpp tocheck;
    int i, j, c, cc;

    cc = 1 << (GGGGC_GENERATION_SIZE - GGGGC_CARD_SIZE);

retry:

    /* first add the roots */
    INIT_BUFFER(tocheck);
    WRITE_BUFFER(tocheck, gcRoots.buf, gcRoots.bufused);

    /* get all the remembered cards */
    for (i = gen + 1; i < GGGGC_GENERATIONS; i++) {
        struct GGGGC_Generation *ggen = ggggc_gens[i];

        for (c = 0; c < cc; c++) {
            if (ggen->remember[c]) {
                /* remembered, add the card */
                struct GGGGC_Header *first = (struct GGGGC_Header *) (((char *) ggen) + (GGGGC_CARD_BYTES * c));
                struct GGGGC_Header *obj = first;

                /* walk through this card */
                while (((size_t) first) == ((size_t) obj & ((size_t) -1 << GGGGC_CARD_SIZE)) && (char *) obj < ggen->top) {
                    void **ptr = (void **) (obj + 1);

                    /* add all its pointers */
                    for (j = 0; j < obj->ptrs; j++, ptr++)
                        WRITE_BUFFER(tocheck, &ptr, 1);

                    obj = (struct GGGGC_Header *) ((char *) obj + obj->sz);
                }

            }
        }
    }

    /* now just iterate while we have things to check */
    for (i = 0; i < tocheck.bufused; i++) {
        void **ptoch = tocheck.buf[i];
        if (*ptoch) {
            struct GGGGC_Header *objtoch = (struct GGGGC_Header *) *ptoch;

            /* OK, we have the object to check, has it already moved? */
            if (objtoch->sz & 1) {
                /* move it */
                *ptoch = (void *) (objtoch->sz & ((size_t) -1 << 1));
                objtoch = (struct GGGGC_Header *) *ptoch;
            }

            /* Do we need to reclaim? */
            if (objtoch->gen <= gen) {
                void **ptr;

                /* nope, get a new one */
                struct GGGGC_Header *newobj =
                    (struct GGGGC_Header *) GGGGC_trymalloc_gen(gen+1, objtoch->sz, objtoch->ptrs);
                if (newobj == NULL) {
                    /* ACK! Out of memory! Need more GC! */
                    FREE_BUFFER(tocheck);
                    gen++;
                    if (gen >= GGGGC_GENERATIONS) {
                        fprintf(stderr, "Memory exhausted during GC???\n");
                        *((int *) 0) = 0;
                    }
                    goto retry;
                }

                /* copy it in */
                memcpy((void *) newobj, (void *) objtoch, objtoch->sz);
                newobj->gen = gen+1;
                objtoch->sz = ((size_t) newobj) | 1; /* forwarding pointer */

                /* and check its pointers */
                ptr = (void **) (newobj + 1);
                for (j = 0; j < newobj->ptrs; j++, ptr++)
                    WRITE_BUFFER(tocheck, &ptr, 1);

                /* finally, update the pointer we're looking at */
                *ptoch = (void *) newobj;
            }
        }
    }

    /* and clear the generations we've done */
    for (i = 0; i <= gen; i++) {
        struct GGGGC_Generation *ggen = ggggc_gens[i];
        ggen->top = (char *) ((size_t) (ggen->remember + cc + GGGGC_CARD_BYTES) & ((size_t) -1 << GGGGC_CARD_SIZE));
    }
}
