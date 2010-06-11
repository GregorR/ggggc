/*
 * Allocation functions
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
#include "helpers.h"

void *GGGGC_trymalloc_gen(unsigned char gen, size_t sz, unsigned char ptrs)
{
    if (gen <= GGGGC_GENERATIONS) {
        struct GGGGC_Generation *ggen = ggggc_gens[gen];

        /* perform the actual allocation */
        if (ggen->top + sz <= ((char *) ggen) + (1<<GGGGC_GENERATION_SIZE) - 1) {
            /* if we allocate at a card boundary, need to mark firstobj */
            size_t c1 = ((size_t) ggen->top & ~((size_t) -1 << GGGGC_GENERATION_SIZE)) >> GGGGC_CARD_SIZE;
            size_t c2 = (((size_t) ggen->top + sz) & ~((size_t) -1 << GGGGC_GENERATION_SIZE)) >> GGGGC_CARD_SIZE;

            /* sufficient room, just give it */
            struct GGGGC_Header *ret = (struct GGGGC_Header *) ggen->top;
            ggen->top += sz;
            memset(ret, 0, sz);
            ret->sz = sz;
            ret->gen = gen;
            ret->ptrs = ptrs;

            if (c1 != c2) {
                ggen->firstobj[c2] = (unsigned char) ((size_t) ggen->top & ~((size_t) -1 << GGGGC_CARD_SIZE));
            }

            return ret;
        }

        /* or fail */
        return NULL;

    } else {
        /* if we fall off, can't safely give anything */
        fprintf(stderr, "Memory exhausted.\n");
        *((int *) 0) = 0;
        return NULL;

    }
}

void *GGGGC_malloc(size_t sz, unsigned char ptrs)
{
    int i;
    void *ret;
    for (i = 0; i <= GGGGC_GENERATIONS; i++) {
        ret = GGGGC_trymalloc_gen(i, sz, ptrs);
        if (ret) return ret;
    }

    /* SHOULD NEVER HAPPEN */
    return NULL;
}

void *GGGGC_malloc_array(size_t sz, size_t nmemb)
{
    return GGGGC_malloc(sizeof(struct GGGGC_Header) + sz * nmemb, nmemb);
}
