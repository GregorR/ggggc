/*
 * Initialization junk
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

#define _BSD_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "ggggc.h"
#include "ggggc_internal.h"
#include "helpers.h"

static void *allocateAligned(size_t sz2)
{
    void *ret;
    size_t sz = 1<<sz2;

    /* mmap double */
    SF(ret, mmap, NULL, (NULL, sz*2, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0));

    /* check for alignment */
    if (((size_t) ret & ~((size_t) -1 << sz2)) != 0) {
        /* not aligned, figure out the proper alignment */
        void *base = (void *) (((size_t) ret + sz) & ((size_t) -1 << sz2));
        munmap(ret, (char *) base - (char *) ret);
        munmap((void *) ((char *) base + sz), sz - ((char *) base - (char *) ret));
        ret = base;
    } else {
        /* aligned, just free the excess */
        munmap((void *) ((char *) ret + sz), sz);
    }

    return ret;
}

void GGGGC_init()
{
    int g, c;

    for (g = 0; g <= GGGGC_GENERATIONS; g++) {
        /* allocate this generation */
        struct GGGGC_Pool *gen = (struct GGGGC_Pool *) allocateAligned(GGGGC_GENERATION_SIZE);

        /* clear out the cards */
        memset(gen->remember, 0, GGGGC_CARDS_PER_GENERATION);

        /* set up the top pointer */
        gen->top = gen->firstobj + GGGGC_CARDS_PER_GENERATION;
        c = (((size_t) gen->top) - (size_t) gen) >> GGGGC_CARD_SIZE;
        gen->firstobj[c] = ((size_t) gen->top) & ~((size_t) -1 << GGGGC_CARD_SIZE);

        ggggc_gens[g] = gen;
    }

    /* other inits */
    GGGGC_collector_init();
}
