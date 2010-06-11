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

#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "ggggc.h"
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

struct GGGGC_Pool *GGGGC_alloc_pool()
{
    size_t c;

    /* allocate this pool */
    struct GGGGC_Pool *pool = (struct GGGGC_Pool *) allocateAligned(GGGGC_GENERATION_SIZE);

    /* clear out the cards */
    memset(pool->remember, 0, GGGGC_CARDS_PER_GENERATION);

    /* set up the top pointer */
    pool->top = pool->firstobj + GGGGC_CARDS_PER_GENERATION;
    c = (((size_t) pool->top) - (size_t) pool) >> GGGGC_CARD_SIZE;
    pool->firstobj[c] = ((size_t) pool->top) & ~((size_t) -1 << GGGGC_CARD_SIZE);

    return pool;
}

struct GGGGC_Generation *GGGGC_alloc_generation(struct GGGGC_Generation *from)
{
    struct GGGGC_Generation *ret;
    size_t sz;
    if (from) {
        sz = sizeof(struct GGGGC_Generation) - sizeof(struct GGGGC_Pool *) + ((from->poolc+1) * sizeof(struct GGGGC_Pool *));
        SF(ret, malloc, NULL, (sz));
        memcpy(ret, from, sz);
        free(from);
        ret->poolc++;
    } else {
        SF(ret, malloc, NULL, (sizeof(struct GGGGC_Generation)));
        ret->poolc = 1;
    }
    ret->pools[ret->poolc-1] = GGGGC_alloc_pool();
    return ret;
}

void *GGGGC_trymalloc_gen(unsigned char gen, size_t sz, unsigned char ptrs)
{
    if (gen <= GGGGC_GENERATIONS) {
        size_t p;
        struct GGGGC_Generation *ggen = ggggc_gens[gen];

        for (p = 0; p <= ggen->poolc; p++) {
            struct GGGGC_Pool *gpool;

            if (p == ggen->poolc) {
                /* need to expand */
                ggggc_gens[gen] = ggen = GGGGC_alloc_generation(ggen);
            }

            gpool = ggen->pools[p];

            /* perform the actual allocation */
            if (gpool->top + sz <= ((char *) gpool) + (1<<GGGGC_GENERATION_SIZE) - 1) {
                /* if we allocate at a card boundary, need to mark firstobj */
                size_t c1 = ((size_t) gpool->top & ~((size_t) -1 << GGGGC_GENERATION_SIZE)) >> GGGGC_CARD_SIZE;
                size_t c2 = (((size_t) gpool->top + sz) & ~((size_t) -1 << GGGGC_GENERATION_SIZE)) >> GGGGC_CARD_SIZE;

                /* sufficient room, just give it */
                struct GGGGC_Header *ret = (struct GGGGC_Header *) gpool->top;
                gpool->top += sz;
                memset(ret, 0, sz);
                ret->sz = sz;
                ret->gen = gen;
                ret->ptrs = ptrs;

                if (c1 != c2) {
                    gpool->firstobj[c2] = (unsigned char) ((size_t) gpool->top & ~((size_t) -1 << GGGGC_CARD_SIZE));
                }

                return ret;
            }
        }

        /* or fail (should be unreachable) */
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
