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

/* to get mmap and MAP_ANON, respectively */
#define _POSIX_C_SOURCE 200112L
#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* figure out what underlying allocator to use */
#if defined(unix) || defined(__unix__) || defined(__unix)
#include <unistd.h> /* for _POSIX_VERSION */
#endif

#if _POSIX_VERSION >= 200112L /* should support mmap */
#include <sys/mman.h>

#ifdef MAP_ANON /* have MAP_ANON, so mmap is fine */
#define FOUND_ALLOCATOR mmap
#define USE_ALLOCATOR_MMAP
#endif

#endif

#if defined(__WIN32) /* use Windows allocators */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define FOUND_ALLOCATOR win32
#define USE_ALLOCATOR_WIN32

#elif !defined(FOUND_ALLOCATOR)
/* don't know what to use, so malloc */
#ifndef GGGGC_MALLOC_OK
#error Cannot find an allocator for this system. Define GGGGC_MALLOC_OK to use malloc.
#endif
#define USE_ALLOCATOR_MALLOC

#endif

#include "ggggc.h"
#include "ggggc_internal.h"
#include "helpers.h"

static void *allocateAligned(size_t sz2)
{
    void *ret;
    size_t sz = 1<<sz2;

#if defined(USE_ALLOCATOR_MMAP)
    /* mmap double */
    SF(ret, mmap, NULL, (NULL, sz*2, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0));
#elif defined(USE_ALLOCATOR_WIN32)
    SF(ret, VirtualAlloc, NULL, (NULL, sz*2, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE));
#elif defined(USE_ALLOCATOR_MALLOC)
    SF(ret, malloc, NULL, (sz*2));
#endif

    /* check for alignment */
    if (((size_t) ret & ~((size_t) -1 << sz2)) != 0) {
        /* not aligned, figure out the proper alignment */
        void *base = (void *) (((size_t) ret + sz) & ((size_t) -1 << sz2));
#if defined(USE_ALLOCATOR_MMAP)
        munmap(ret, (char *) base - (char *) ret);
        munmap((void *) ((char *) base + sz), sz - ((char *) base - (char *) ret));
#elif defined(USE_ALLOCATOR_WIN32)
        VirtualFree(ret, (char *) base - (char *) ret, MEM_RELEASE);
        VirtualFree((void *) ((char *) base + sz), sz - ((char *) base - (char *) ret), MEM_RELEASE);
#endif
        ret = base;

    } else {
        /* aligned, just free the excess */
#if defined(USE_ALLOCATOR_MMAP)
        munmap((void *) ((char *) ret + sz), sz);
#elif defined(USE_ALLOCATOR_WIN32)
        VirtualFree((void *) ((char *) ret + sz), sz, MEM_RELEASE);
#endif
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
    pool->remaining = GGGGC_POOL_BYTES - ((size_t) pool->top - (size_t) pool);
#if defined(GGGGC_DEBUG) || defined(GGGGC_CLEAR_POOLS)
    memset(pool->top, 0, pool->remaining);
#endif
    /*pool->remc = GGGGC_CARD_BYTES - ((size_t) pool->top & GGGGC_CARD_MASK);*/

    /* first object in the first card */
    c = (((size_t) pool->top) - (size_t) pool) >> GGGGC_CARD_SIZE;
    pool->firstobj[c] = ((size_t) pool->top) & GGGGC_CARD_MASK;
}

struct GGGGC_Pool *GGGGC_alloc_pool()
{
    /* allocate this pool */
    int tmpi;
    struct GGGGC_Pool *pool = (struct GGGGC_Pool *) allocateAligned(GGGGC_POOL_SIZE);
    pool->lock = GGC_alloc_mutex();
    GGC_MUTEX_INIT(tmpi, pool->lock);
    pool->next = NULL;
    GGGGC_clear_pool(pool);

    return pool;
}

static __inline__ void *GGGGC_trymalloc_pool(unsigned char gen, struct GGGGC_Pool *gpool, size_t sz, unsigned short ptrs)
{
    /* perform the actual allocation */
    size_t remaining = gpool->remaining;
    if (sz < remaining) {
        size_t c1, c2;
        struct GGGGC_Header *ret;
        void **pt;
        char *top;

        /* reduce it */
        while (!GGC_cas(gpool->lock, (void **) &gpool->remaining, (void *) remaining, (void *) (remaining - sz))) {
            remaining = gpool->remaining;
            if (remaining <= sz) return NULL;
        }

        /* current top */
        top = gpool->top;
        c1 = GGGGC_CARD_OF(top);

        /* try to increase the top */
        while (!GGC_cas(gpool->lock, (void **) &gpool->top, (void *) top, (void *) (top + sz))) {
            top = gpool->top;
        }

        /* sufficient room, just give it */
        ret = (struct GGGGC_Header *) top;
        top += sz;
#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
        ret->magic = GGGGC_HEADER_MAGIC;
#endif
        ret->sz = sz;
        ret->gen = gen;
        ret->type = 0;
        ret->ptrs = ptrs;

        /* clear out pointers */
        pt = (void *) (ret + 1);
        while (ptrs--) *pt++ = NULL;

        /* if we allocate at a card boundary, need to mark firstobj */
        if (gen != 0) {
            c2 = GGGGC_CARD_OF(top);
            if (c1 != c2)
                gpool->firstobj[c2] = (unsigned char) ((size_t) top & GGGGC_CARD_MASK);
        }

        return (void *) (ret + 1);
    }

    return NULL;
}

void *GGGGC_trymalloc_gen(unsigned char gen, int expand, size_t sz, unsigned short ptrs)
{
    struct GGGGC_Pool *gpool = ggggc_gens[gen];
    void *ret;

    if ((ret = GGGGC_trymalloc_pool(gen, gpool, sz, ptrs))) return ret;

    /* crazy loops to avoid overchecking */
retry:
    for (; gpool->next; gpool = gpool->next) {
        if ((ret = GGGGC_trymalloc_pool(gen, gpool->next, sz, ptrs))) return ret;
    }

    /* failed to find, must expand */
    if (!expand)
        return NULL;

    /* need to expand */
    gpool->next = GGGGC_alloc_pool();
    goto retry;
}

static __inline__ void *GGGGC_trymalloc_gen0(size_t sz, unsigned short ptrs)
{
    struct GGGGC_Pool *gpool = ggggc_gens[0];
    void *ret;

    /* call this only after trying ggggc_allocpool, so probably pool[0] */
retry:
    for (; gpool->next; gpool = gpool->next) {
        if ((ret = GGGGC_trymalloc_pool(0, gpool->next, sz, ptrs))) {
            ggggc_allocpool = gpool->next;
            return ret;
        }
    }

    /* need to expand */
    gpool->next = GGGGC_alloc_pool();
    if (sz >= gpool->next->remaining) {
        /* there will never be enough room! */
        fprintf(stderr, "Allocation of a too-large object: %d > %d\n", (int) sz, (int) gpool->next->remaining);
        return NULL;
    }
    goto retry;
}

void *GGGGC_malloc(size_t sz, unsigned short ptrs)
{
    void *ret;
    if ((ret = GGGGC_trymalloc_pool(0, ggggc_allocpool, sz, ptrs))) return ret;
    return GGGGC_trymalloc_gen0(sz, ptrs);
}

void *GGGGC_malloc_ptr_array(size_t sz)
{
    return GGGGC_malloc(sizeof(struct GGGGC_Header) + sz * sizeof(void *), sz);
}

void *GGGGC_realloc_ptr_array(void *orig, size_t sz)
{
    struct GGGGC_Header *header = (struct GGGGC_Header *) orig - 1;
    void *ret;

    /* just allocate and copy out the original */
    ret = GGGGC_malloc_ptr_array(sz);
    sz = sz * sizeof(void *);
    if (header->sz - sizeof(struct GGGGC_Header) < sz) {
        memcpy(ret, orig, header->sz - sizeof(struct GGGGC_Header));
    } else {
        memcpy(ret, orig, sz);
    }

    return ret;
}

void *GGGGC_malloc_data_array(size_t sz)
{
    /* in this case, we could try to allocate unaligned. Yuck! */
    if (sz % sizeof(void *) != 0) {
        sz += sizeof(void *);
        sz -= sz % sizeof(void *);
    }
    return GGGGC_malloc(sz, 0);
}

void *GGGGC_realloc_data_array(void *orig, size_t sz)
{
    struct GGGGC_Header *header = (struct GGGGC_Header *) orig - 1;
    void *ret;

    /* allocate and copy */
    ret = GGGGC_malloc_data_array(sz);
    if (header->sz - sizeof(struct GGGGC_Header) < sz) {
        memcpy(ret, orig, header->sz - sizeof(struct GGGGC_Header));
    } else {
        memcpy(ret, orig, sz);
    }

    return ret;
}
