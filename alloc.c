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

void GGGGC_clear_pool(struct GGGGC_Pool *pool)
{
    size_t c;

    /* clear it out */
    memset(pool->remember, 0, GGGGC_CARDS_PER_POOL);

    /* set up the top pointer */
    pool->top = pool->firstobj + GGGGC_CARDS_PER_POOL;

    /* remaining amount */
    /*pool->remaining = GGGGC_POOL_BYTES - (pool->top - (char *) pool);
    pool->remc = GGGGC_CARD_BYTES - ((size_t) pool->top & GGGGC_CARD_MASK);*/

    /* first object in the first card */
    c = (((size_t) pool->top) - (size_t) pool) >> GGGGC_CARD_SIZE;
    pool->firstobj[c] = ((size_t) pool->top) & GGGGC_CARD_MASK;
}

struct GGGGC_Pool *GGGGC_alloc_pool()
{
    /* allocate this pool */
    struct GGGGC_Pool *pool = (struct GGGGC_Pool *) allocateAligned(GGGGC_POOL_SIZE);
    GGGGC_clear_pool(pool);

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

static void *GGGGC_trymalloc_pool(unsigned char gen, struct GGGGC_Pool *gpool, size_t sz, unsigned char ptrs)
{
    /* perform the actual allocation */
    if (gpool->top + sz < ((char *) gpool) + GGGGC_POOL_BYTES) {
        /* if we allocate at a card boundary, need to mark firstobj */
        size_t c1 = ((size_t) gpool->top & GGGGC_POOL_MASK) >> GGGGC_CARD_SIZE;
        size_t c2 = (((size_t) gpool->top + sz) & GGGGC_POOL_MASK) >> GGGGC_CARD_SIZE;

        /* sufficient room, just give it */
        struct GGGGC_Header *ret = (struct GGGGC_Header *) gpool->top;
        gpool->top += sz;
        memset(ret, 0, sz);
        ret->sz = sz;
        ret->gen = gen;
        ret->ptrs = ptrs;

        if (c1 != c2)
            gpool->firstobj[c2] = (unsigned char) ((size_t) gpool->top & GGGGC_CARD_MASK);

        return ret;
    }

    return NULL;
}

void *GGGGC_trymalloc_gen(unsigned char gen, int expand, size_t sz, unsigned char ptrs)
{
    size_t p;
    struct GGGGC_Generation *ggen = ggggc_gens[gen];
    void *ret;

    if ((ret = GGGGC_trymalloc_pool(gen, ggen->pools[0], sz, ptrs))) return ret;

    for (p = 1; p <= ggen->poolc; p++) {
        if (p == ggen->poolc) {
            if (!expand)
                return NULL;

            /* need to expand */
            ggggc_gens[gen] = ggen = GGGGC_alloc_generation(ggen);
        }

        if ((ret = GGGGC_trymalloc_pool(gen, ggen->pools[p], sz, ptrs))) return ret;
    }

    /* or fail (should be unreachable) */
    return NULL;
}

void *GGGGC_malloc(size_t sz, unsigned char ptrs)
{
    void *ret;
    if ((ret = GGGGC_trymalloc_pool(0, ggggc_pool0, sz, ptrs))) return ret;
    return GGGGC_trymalloc_gen(0, 1, sz, ptrs);
}

void *GGGGC_malloc_ptr_array(size_t sz, size_t nmemb)
{
    return GGGGC_malloc(sizeof(struct GGGGC_Header) + sz * nmemb, nmemb);
}

void *GGGGC_malloc_data_array(size_t sz, size_t nmemb)
{
    return GGGGC_malloc(sizeof(struct GGGGC_Header) + sz * nmemb, 0);
}
