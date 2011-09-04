/*
 * Allocation functions
 *
 * Copyright (C) 2010, 2011 Gregor Richards
 * Copyright (C) 2011 Elliott Hird
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
#define _DARWIN_C_SOURCE /* for MAP_ANON on OS X */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* figure out what underlying allocator to use */
#if defined(unix) || defined(__unix__) || defined(__unix)
#include <unistd.h> /* for _POSIX_VERSION */
#endif

#if _POSIX_VERSION >= 200112L || defined(__APPLE__) /* should support mmap */
#include <sys/mman.h>

#if defined(MAP_ANON) /* have MAP_ANON, so mmap is fine */
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
    pool->top = (char *) (pool->firstobj + GGGGC_CARDS_PER_POOL);

    /* remaining amount */
#if defined(GGGGC_DEBUG) || defined(GGGGC_CLEAR_POOLS)
    memset(pool->top, 0, GGGGC_POOL_BYTES - ((size_t) pool->top - (size_t) pool));
#endif
    /*pool->remc = GGGGC_CARD_BYTES - ((size_t) pool->top & GGGGC_CARD_MASK);*/

    /* first object in the first card */
    c = (((size_t) pool->top) - (size_t) pool) >> GGGGC_CARD_SIZE;
    pool->firstobj[c] = ((size_t) pool->top) & GGGGC_CARD_MASK;
}

struct GGGGC_Pool *GGGGC_alloc_pool()
{
    /* allocate this pool */
    struct GGGGC_Pool *pool = (struct GGGGC_Pool *) allocateAligned(GGGGC_POOL_SIZE);

    if (pool) {
        pool->next = NULL;
        GGGGC_clear_pool(pool);
    }

    return pool;
}

void GGGGC_free_pool(struct GGGGC_Pool *pool)
{
#if defined(USE_ALLOCATOR_MMAP)
    munmap(pool, GGGGC_POOL_BYTES);
#elif defined(USE_ALLOCATOR_WIN32)
    VirtualFree(pool, GGGGC_POOL_BYTES, MEM_RELEASE);
#else
    free(pool);
#endif
}

static __inline__ void *GGGGC_trymalloc_pool(unsigned char gen, struct GGGGC_Pool *gpool, size_t sz, unsigned short ptrs)
{
    /* perform the actual allocation */
    if (LIKELY((void *) (((size_t) gpool->top + sz) & GGGGC_NOPOOL_MASK) == gpool)) {
        struct GGGGC_Header *ret;
        size_t c1, c2;

        /* current top */
        ret = (struct GGGGC_Header *) gpool->top;
        gpool->top += sz;

        /* if we allocate /right/ at a card boundary (header before, content after), need to move */
        c1 = GGGGC_CARD_OF(ret);
        c2 = GGGGC_CARD_OF(ret + 1);
        if (UNLIKELY(c1 != c2)) {
            /* we allocated /right/ on the edge of a card, which means the wrong card will be marked! Choose the next one instead */
            fprintf(stderr, "Whoops %p\n", (void *) (ret+1));
            ret->sz = (size_t) -1;
            ret++;
            c1 = GGGGC_CARD_OF(ret);
            gpool->top += sizeof(struct GGGGC_Header);
            c1 = c2;
            gpool->firstobj[c1] = (unsigned short) ((size_t) gpool->top & GGGGC_CARD_MASK);
        }

        /* if we allocate at a card boundary, need to mark firstobj */
        c2 = GGGGC_CARD_OF(gpool->top);
        if (c1 != c2)
            gpool->firstobj[c2] = (unsigned short) ((size_t) gpool->top & GGGGC_CARD_MASK);

        /* set this so when we do card-searches, we know when to stop */
        ((struct GGGGC_Header *) gpool->top)->sz = (size_t) -1;

        return (void *) (ret + 1);
    }

    return NULL;
}

static __inline__ void *GGGGC_trymalloc_pool0(struct GGGGC_Pool *gpool, size_t sz, unsigned short ptrs)
{
    /* perform the actual allocation in gen0 */
    if (LIKELY((void *) (((size_t) gpool->top + sz) & GGGGC_NOPOOL_MASK) == gpool)) {
        struct GGGGC_Header *ret;
        void **pt;

        /* current top */
        ret = (struct GGGGC_Header *) gpool->top;
        gpool->top += sz;

        /* configure it if this is a direct mutator allocation */
#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
        ret->magic = GGGGC_HEADER_MAGIC;
#endif
        ret->sz = sz;
        ret->gen = 0;
        ret->ptrs = ptrs;

        pt = (void *) (ret + 1);
        while (ptrs--) *pt++ = NULL;

        return (void *) (ret + 1);
    }

    return NULL;
}

void *GGGGC_trymalloc_gen(unsigned char gen, int expand, struct GGGGC_Pool **allocpool, size_t sz, unsigned short ptrs)
{
    struct GGGGC_Pool *gpool = *allocpool;
    void *ret;

    /* if no allocation pool was provided, default */
    if (gpool == NULL) {
        gpool = *allocpool = ggggc_gens[gen];
    }

    if ((ret = GGGGC_trymalloc_pool(gen, gpool, sz, ptrs))) return ret;

    /* crazy loops to avoid overchecking */
retry:
    for (; gpool->next; gpool = gpool->next) {
        if ((ret = GGGGC_trymalloc_pool(gen, gpool->next, sz, ptrs))) {
            *allocpool = gpool->next;
            return ret;
        }
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
    struct GGGGC_Pool *gpool = ggggc_allocpool;
    void *ret;

    /* call this only after trying ggggc_allocpool, so probably pool[0] */
retry:
    for (; gpool->next; gpool = gpool->next) {
        if ((ret = GGGGC_trymalloc_pool0(gpool->next, sz, ptrs))) {
            ggggc_allocpool = gpool->next;
            return ret;
        }
    }
    if ((ret = GGGGC_trymalloc_pool0(gpool, sz, ptrs))) {
        ggggc_allocpool = gpool;
        return ret;
    }

    /* need to expand */
    gpool->next = GGGGC_alloc_pool();
    if ((void *) (((size_t) gpool->next->top + sz) & GGGGC_NOPOOL_MASK) != (void *) gpool->next) {
        /* there will never be enough room! */
        fprintf(stderr, "Allocation of a too-large object: %d\n", (int) sz);
        return NULL;
    }
    goto retry;
}

void *GGGGC_malloc(size_t sz, unsigned short ptrs)
{
    void *ret;
    if (LIKELY(ret = GGGGC_trymalloc_pool0(ggggc_allocpool, sz, ptrs))) return ret;
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

/* Globalize a pstack member */
struct GGGGC_PStack *GGGGC_globalizePStack(struct GGGGC_PStack *pstack)
{
    size_t ct, i;
    struct GGGGC_PStack *ret;

    /* count them */
    for (ct = 0; pstack->ptrs[ct]; ct++);

    /* then allocate */
    SF(ret, malloc, NULL, ((ct + 2) * sizeof(void *)));
    ret->next = pstack->next;
    for (i = 0; i <= ct; i++) ret->ptrs[i] = pstack->ptrs[i];

    return ret;
}
