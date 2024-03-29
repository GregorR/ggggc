/*
 * Functions related to allocation of pools and various data structures, but
 * not objects themselves, the implementation of which is in the collector
 * implementation files.
 *
 * Copyright (c) 2014-2023 Gregor Richards
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define _BSD_SOURCE /* for MAP_ANON */
#define _DEFAULT_SOURCE /* for glibc */
#define _DARWIN_C_SOURCE /* for MAP_ANON on OS X */

/* for standards info */
#if defined(unix) || defined(__unix) || defined(__unix__) || \
    (defined(__APPLE__) && defined(__MACH__))
#include <unistd.h>
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#if _POSIX_VERSION
#include <sys/mman.h>
#endif

#include "ggggc/gc.h"
#include "ggggc-internals.h"

#ifdef __cplusplus
extern "C" {
#endif

/* figure out which allocator to use */
#if defined(GGGGC_USE_MALLOC)
#define GGGGC_ALLOCATOR_MALLOC 1
#include "allocator/malloc.c"

#elif defined(GGGGC_USE_SBRK)
#define GGGGC_ALLOCATOR_SBRK 1
#include "allocator/sbrk.c"

#elif defined(GGGGC_USE_PORTABLE_ALLOCATOR)
#define GGGGC_ALLOCATOR_PORTABLE 1
#include "allocator/portable.c"

#elif _POSIX_ADVISORY_INFO >= 200112L
#define GGGGC_ALLOCATOR_POSIX_MEMALIGN 1
#include "allocator/malign.c"

#elif defined(MAP_ANON)
#define GGGGC_ALLOCATOR_MMAP 1
#include "allocator/mmap.c"

#elif defined(_WIN32)
#define GGGGC_ALLOCATOR_VIRTUALALLOC 1
#include "allocator/win-valloc.c"

#else
#define GGGGC_ALLOCATOR_SBRK 1
#include "allocator/sbrk.c"

#endif

/* pools which are freely available */
static ggc_mutex_t freePoolsLock = GGC_MUTEX_INITIALIZER;
static struct GGGGC_Pool *freePoolsHead, *freePoolsTail;

/* allocate and initialize a pool */
struct GGGGC_Pool *ggggc_newPool(int mustSucceed)
{
    struct GGGGC_Pool *ret;
#ifdef GGGGC_DEBUG_TINY_HEAP
    static ggc_thread_local int allocationsLeft = GGGGC_GENERATIONS;

    if (allocationsLeft-- <= 0) {
        allocationsLeft = 0;
        if (mustSucceed) {
            fprintf(stderr, "GGGGC: exceeded tiny heap size\n");
            abort();
        }
        return NULL;
    }
#endif

    ret = NULL;

    /* try to reuse a pool */
    if (freePoolsHead) {
        ggc_mutex_lock_raw(&freePoolsLock);
        if (freePoolsHead) {
            ret = freePoolsHead;
            freePoolsHead = freePoolsHead->next;
            if (!freePoolsHead) freePoolsTail = NULL;
        }
        ggc_mutex_unlock(&freePoolsLock);
    }

    /* otherwise, allocate one */
    if (!ret) ret = (struct GGGGC_Pool *) allocPool(mustSucceed);

    if (!ret) return NULL;

    /* set it up */
    ret->next = NULL;
    ret->free = ret->start;
    ret->end = (ggc_size_t *) ((unsigned char *) ret + GGGGC_POOL_BYTES);

    return ret;
}

/* heuristically expand a pool list if it has too many survivors
 * poolList: Pool list to expand
 * newPool: Function to allocate a new pool based on a prototype pool
 * ratio: As a power of two, portion of pool that should be survivors */
void ggggc_expandPoolList(struct GGGGC_Pool *poolList,
                          struct GGGGC_Pool *(*newPool)(struct GGGGC_Pool *),
                          int ratio)
{
    struct GGGGC_Pool *pool = poolList;
    ggc_size_t space, survivors, poolCt;

    if (!pool) return;

    /* first figure out how much space was used */
    space = 0;
    survivors = 0;
    poolCt = 0;
    while (1) {
        space += pool->end - pool->start;
        survivors += pool->survivors;
        pool->survivors = 0;
        poolCt++;
        if (!pool->next) break;
        pool = pool->next;
    }

    /* now decide if it's too much */
    if ((survivors<<ratio) > space) {
        /* allocate more */
        ggc_size_t i;
        for (i = 0; i < poolCt; i++) {
            pool->next = newPool(poolList);
            pool = pool->next;
            if (!pool) break;
        }
    }
}

/* free a generation (used when a thread exits) */
void ggggc_freeGeneration(struct GGGGC_Pool *pool)
{
    if (!pool) return;
    ggc_mutex_lock_raw(&freePoolsLock);
    if (freePoolsHead) {
        freePoolsTail->next = pool;
    } else {
        freePoolsHead = pool;
    }
    while (pool->next) pool = pool->next;
    freePoolsTail = pool;
    ggc_mutex_unlock(&freePoolsLock);
}

struct GGGGC_Array {
    struct GGGGC_Header header;
    ggc_size_t length;
};

/* allocate a pointer array (size is in words) */
void *ggggc_mallocPointerArray(ggc_size_t sz)
{
    struct GGGGC_Descriptor *descriptor = ggggc_allocateDescriptorPA(sz + 1 + sizeof(struct GGGGC_Header)/sizeof(ggc_size_t));
    struct GGGGC_Array *ret = (struct GGGGC_Array *) ggggc_malloc(descriptor);
    ret->length = sz;
    return ret;
}

/* allocate a data array */
void *ggggc_mallocDataArray(ggc_size_t nmemb, ggc_size_t size)
{
    ggc_size_t sz = ((nmemb*size)+sizeof(ggc_size_t)-1)/sizeof(ggc_size_t);
    struct GGGGC_Descriptor *descriptor = ggggc_allocateDescriptorDA(
        sz + 1 + GGGGC_WORD_SIZEOF(struct GGGGC_Header));
    struct GGGGC_Array *ret = (struct GGGGC_Array *) ggggc_malloc(descriptor);
    ret->length = nmemb;
    return ret;
}

/* allocate a descriptor-descriptor for a descriptor of the given size */
struct GGGGC_Descriptor *ggggc_allocateDescriptorDescriptor(ggc_size_t size)
{
    struct GGGGC_Descriptor *ret, *ddd = NULL;
    ggc_size_t ddSize;

    /* check for minimum size */
    if (size < GGGGC_MINIMUM_OBJECT_SIZE)
        size = GGGGC_MINIMUM_OBJECT_SIZE;

    /* check if we already have a descriptor */
    if (ggggc_descriptorDescriptors[size])
        return ggggc_descriptorDescriptors[size];

    /* need one description bit for every word in the object */
    ddSize = GGGGC_WORD_SIZEOF(struct GGGGC_Descriptor) + GGGGC_DESCRIPTOR_WORDS_REQ(size) - 1;

    /* get our descriptor-descriptor-descriptor first */
    if (ddSize != size)
        ddd = ggggc_allocateDescriptorDescriptor(ddSize);

    /* otherwise, need to allocate one. First lock the space */
    ggc_mutex_lock_raw(&ggggc_descriptorDescriptorsLock);
    if (ggggc_descriptorDescriptors[size]) {
        ggc_mutex_unlock(&ggggc_descriptorDescriptorsLock);
        return ggggc_descriptorDescriptors[size];
    }

    /* allocate the descriptor descriptor */
    ret = (struct GGGGC_Descriptor *) ggggc_mallocRaw(&ddd, ddSize);

    /* make it correct */
    if (ddSize != size)
        ret->header.descriptor__ptr = ddd;
    else
        ret->header.descriptor__ptr = ret;
    ret->size = size;
#ifndef GGGGC_FEATURE_EXTTAG
    ret->pointers[0] = GGGGC_DESCRIPTOR_DESCRIPTION;
#else
    memset(ret->tags, 1, size);
    ret->tags[0] = 0;
    ret->tags[GGGGC_OFFSETOF(struct GGGGC_Descriptor *, user)] = 0;
#endif

    /* put it in the list */
    ggggc_descriptorDescriptors[size] = ret;
    ggc_mutex_unlock(&ggggc_descriptorDescriptorsLock);
    {
        GGC_PUSH_1(ggggc_descriptorDescriptors[size]);
        GGC_GLOBALIZE();
    }

    return ret;
}

/* allocate a descriptor for an object of the given size in words with the
 * given pointer layout */
struct GGGGC_Descriptor *ggggc_allocateDescriptor(ggc_size_t size, ggc_size_t pointers)
{
    ggc_size_t pointersA[1];
    pointersA[0] = pointers;
    return ggggc_allocateDescriptorL(size, pointersA);
}

/* descriptor allocator when more than one word is required to describe the
 * pointers */
struct GGGGC_Descriptor *ggggc_allocateDescriptorL(ggc_size_t size, const ggc_size_t *pointers)
{
    struct GGGGC_Descriptor *dd, *ret;
    ggc_size_t dPWords, dSize;

    /* check for minimum size */
    /* FIXME: Theoretically if our minimum size straddled a word length, this
     * would cause us to read past the end of pointers */
    if (size < GGGGC_MINIMUM_OBJECT_SIZE)
        size = GGGGC_MINIMUM_OBJECT_SIZE;

    /* the size of the descriptor */
    if (pointers)
        dPWords = GGGGC_DESCRIPTOR_WORDS_REQ(size);
    else
        dPWords = 1;
    dSize = GGGGC_WORD_SIZEOF(struct GGGGC_Descriptor) + dPWords - 1;

    /* get a descriptor-descriptor for the descriptor we're about to allocate */
    dd = ggggc_allocateDescriptorDescriptor(dSize);

    /* use that to allocate the descriptor */
    ret = (struct GGGGC_Descriptor *) ggggc_mallocRaw(&dd, dd->size);
    ret->header.descriptor__ptr = dd;
    ret->size = size;

    /* and set it up */
#ifndef GGGGC_FEATURE_EXTTAG
    if (pointers) {
        memcpy(ret->pointers, pointers, sizeof(ggc_size_t) * dPWords);
        ret->pointers[0] |= 1; /* first word is always the descriptor pointer */
    } else {
        ret->pointers[0] = 0;
    }

#else /* !GGGGC_FEATURE_EXTTAG */
    if (pointers) {
        memset(ret->tags, 1, size);
        ggc_size_t pi, si, curP, curM;
        pi = -1;
        curM = 0;
        for (si = 0; si < size; si++) {
            if (!curM) {
                curP = pointers[++pi];
                curM = 1;
            }
            if (curP & curM)
                ret->tags[si] = 0;
            curM <<= 1;
        }
        ret->tags[0] = 0; /* first word is always the descriptor pointer */
    } else {
        ret->tags[0] = 1;
    }

#endif /* GGGGC_FEATURE_EXTTAG */

    return ret;
}

#ifdef GGGGC_FEATURE_EXTTAG
/* descriptor allocator when deeper tag information than presence of pointers
 * is provided */
struct GGGGC_Descriptor *ggggc_allocateDescriptorT(ggc_size_t size, const unsigned char *tags)
{
    struct GGGGC_Descriptor *dd, *ret;
    ggc_size_t dPWords, dSize;

    /* check for minimum size */
    /* FIXME: Theoretically if our minimum size straddled a word length, this
     * would cause us to read past the end of pointers */
    if (size < GGGGC_MINIMUM_OBJECT_SIZE)
        size = GGGGC_MINIMUM_OBJECT_SIZE;

    /* the size of the descriptor */
    if (tags)
        dPWords = GGGGC_DESCRIPTOR_WORDS_REQ(size);
    else
        dPWords = 1;
    dSize = GGGGC_WORD_SIZEOF(struct GGGGC_Descriptor) + dPWords - 1;

    /* get a descriptor-descriptor for the descriptor we're about to allocate */
    dd = ggggc_allocateDescriptorDescriptor(dSize);

    /* use that to allocate the descriptor */
    ret = (struct GGGGC_Descriptor *) ggggc_mallocRaw(&dd, dd->size);
    ret->header.descriptor__ptr = dd;
    ret->size = size;

    /* and set it up */
    if (tags) {
        memcpy(ret->tags, tags, size);
        ret->tags[0] = 0; /* first word is always the descriptor pointer */
    } else {
        ret->tags[0] = 1;
    }

    return ret;
}
#endif

/* descriptor allocator for pointer arrays */
struct GGGGC_Descriptor *ggggc_allocateDescriptorPA(ggc_size_t size)
{
    ggc_size_t *pointers;
    ggc_size_t dPWords, i;
    struct GGGGC_Descriptor *ret;

    /* fill our pointer-words with 1s */
    dPWords = GGGGC_DESCRIPTOR_WORDS_REQ(size);
#ifdef alloca
    pointers = (ggc_size_t *)
        alloca(sizeof(ggc_size_t) * dPWords);
#else
    pointers = (ggc_size_t *)
        malloc(sizeof(ggc_size_t) * dPWords);
    if (!pointers) abort();
#endif
    for (i = 0; i < dPWords; i++) pointers[i] = (ggc_size_t) -1;

    /* get rid of non-pointers */
    pointers[0] &= ~(
        ((ggc_size_t)1<<(((ggc_size_t) (void *) &((struct GGGGC_Header *) 0)->descriptor__ptr)/sizeof(ggc_size_t))) |
#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
        ((ggc_size_t)1<<(((ggc_size_t) (void *) &((struct GGGGC_Header *) 0)->ggggc_memoryCorruptionCheck)/sizeof(ggc_size_t))) |
#endif
        ((ggc_size_t)1<<((((ggc_size_t) (void *) &((GGC_voidpArray) 0)->length)/sizeof(ggc_size_t))))
        );

    /* and allocate */
    ret = ggggc_allocateDescriptorL(size, pointers);
#ifndef alloca
    free(pointers);
#endif
    return ret;
}

/* descriptor allocator for data arrays */
struct GGGGC_Descriptor *ggggc_allocateDescriptorDA(ggc_size_t size)
{
    /* and allocate */
    return ggggc_allocateDescriptorL(size, NULL);
}

/* allocate a descriptor from a descriptor slot */
struct GGGGC_Descriptor *ggggc_allocateDescriptorSlot(struct GGGGC_DescriptorSlot *slot)
{
    if (slot->descriptor) return slot->descriptor;
    ggc_mutex_lock_raw(&slot->lock);
    if (slot->descriptor) {
        ggc_mutex_unlock(&slot->lock);
        return slot->descriptor;
    }

    slot->descriptor = ggggc_allocateDescriptor(slot->size, slot->pointers);
    ggc_mutex_unlock(&slot->lock);

    /* make the slot descriptor a root */
    {
        GGC_PUSH_1(slot->descriptor);
        GGC_GLOBALIZE();
    }

    return slot->descriptor;
}

/* and a combined malloc/allocslot */
void *ggggc_mallocSlot(struct GGGGC_DescriptorSlot *slot)
{
    return ggggc_malloc(ggggc_allocateDescriptorSlot(slot));
}

#ifdef GGGGC_FEATURE_FINALIZERS
/* specify a finalizer for an object */
void ggggc_finalize(void *obj, ggc_finalizer_t finalizer)
{
    GGGGC_FinalizerEntry entry = NULL, next = NULL;
    struct GGGGC_Pool *pool = NULL;

    GGC_PUSH_3(obj, entry, next);

    /* allocate the entry */
    /* NOTE: This is the ONLY GC_NEW in the entire garbage collector, since
     * finalizer entries are the only high-level data type needed by the
     * collector itself. But, as a consequence, we can't count on descriptors
     * being pre-constructed, since the constructors for descriptors may not
     * have actually run yet. As such, we use GGC_NEW_FROM_DESCRIPTOR_SLOT here,
     * which will allocate the descriptor if needed, instead of just GGC_NEW. */
    entry = (GGGGC_FinalizerEntry)
        GGC_NEW_FROM_DESCRIPTOR_SLOT(&GGGGC_FinalizerEntry__descriptorSlot);

    /* set it up */
    GGC_WP(entry, obj, obj);
    GGC_WD(entry, finalizer, finalizer);

    /* add it to the list */
    pool = GGGGC_POOL_OF(entry);
    next = (GGGGC_FinalizerEntry) pool->finalizers;
    GGC_WP(entry, next, next);
    pool->finalizers = entry;
}

/* and function for running finalizers */
void ggggc_runFinalizers(GGGGC_FinalizerEntry finalizers)
{
    GGGGC_FinalizerEntry finalizer = NULL;
    void *obj = NULL;
    ggc_finalizer_t ffunc;

    GGC_PUSH_3(finalizers, finalizer, obj);

    finalizer = finalizers;
    while (finalizer) {
        obj = GGC_RP(finalizer, obj);
        ffunc = GGC_RD(finalizer, finalizer);
        ffunc(obj);

        finalizer = GGC_RP(finalizer, next);
    }
}
#endif /* GGGGC_FEATURE_FINALIZERS */

#ifdef __cplusplus
}
#endif
