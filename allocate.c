/*
 * Allocation functions
 *
 * Copyright (c) 2014 Gregor Richards
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

/* for standards info */
#if defined(unix) || defined(__unix) || defined(__unix__)
#include <unistd.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>

/* figure out which allocator to use */
#if _POSIX_ADVISORY_INFO >= 200112L
#define GGGGC_ALLOCATOR_POSIX_MEMALIGN 1

#elif defined(MAP_ANON)
#define GGGGC_ALLOCATOR_MMAP 1

#else
#warning GGGGC: No allocator available other than malloc!
#define GGGGC_ALLOCATOR_MALLOC 1

#endif

#include "ggggc/gc.h"
#include "ggggc-internals.h"

/* allocate a pool */
static struct GGGGC_Pool *newPool(unsigned char gen)
{
    struct GGGGC_Pool *ret;

#if GGGGC_ALLOCATOR_POSIX_MEMALIGN
    if ((errno = posix_memalign((void **) &ret, GGGGC_POOL_BYTES, GGGGC_POOL_BYTES))) {
        /* FIXME: be smarter */
        perror("posix_memalign");
        exit(1);
    }

#elif GGGGC_ALLOCATOR_MMAP
    unsigned char *space, *aspace;

    /* allocate enough space that we can align it later */
    space = mmap(NULL, GGGGC_POOL_BYTES*2, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
    if (space == NULL) {
        /* FIXME: shouldn't just die */
        perror("mmap");
        exit(1);
    }

    /* align it */
    ret = GGGGC_POOL_OF(space + GGGGC_POOL_BYTES - 1);
    aspace = (unsigned char *) ret;

    /* free unused space */
    if (aspace > space)
        munmap(space, aspace - space);
    munmap(aspace + GGGGC_POOL_BYTES, space + GGGGC_POOL_BYTES - aspace);

#elif GGGGC_ALLOCATOR_MALLOC
    unsigned char *space;

    space = malloc(GGGGC_POOL_BYTES*2);
    if (space == NULL) {
        perror("malloc");
        exit(1);
    }

    ret = GGGGC_POOL_OF(space + GGGGC_POOL_BYTES - 1);

#else
#error Unknown allocator.
#endif

    /* space reserved, now set it up */
    ret->gen = gen;
    ret->free = ret->start;
    ret->end = (size_t *) ((unsigned char *) ret + GGGGC_POOL_BYTES);

    /* clear the remembered set */
    if (gen > 0)
        memset(ret->remember, 0, GGGGC_CARDS_PER_POOL);

    /* the first object in the first usable card */
    ret->firstObject[GGGGC_CARD_OF(ret->start)] =
        (((size_t) ret->start) & GGGGC_CARD_INNER_MASK) / sizeof(size_t);

    return ret;
}

/* heuristically expand a generation if it has too many survivors */
void ggggc_expandGeneration(struct GGGGC_Pool *pool)
{
    size_t space, survivors, poolCt;

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
    if (survivors > space/2) {
        /* allocate more */
        size_t i;
        for (i = 0; i < poolCt; i++) {
            pool->next = newPool(pool->gen);
            pool = pool->next;
        }
    }
}

/* NOTE: there is code duplication between ggggc_mallocGen0 and
 * ggggc_mallocGenN because I can't trust a compiler to inline and optimize for
 * the 0 case */

/* allocate an object in generation 0 */
void *ggggc_mallocGen0(struct GGGGC_UserTypeInfo *uti, /* the object to allocate */
                       int force /* allocate a new pool instead of collecting, if necessary */
                       ) {
    struct GGGGC_Pool *pool;
    struct GGGGC_Header *ret;
    struct GGGGC_Descriptor *descriptor;

retry:
    /* get our allocation pool */
    if (ggggc_pool0) {
        pool = ggggc_pool0;
    } else {
        ggggc_gen0 = ggggc_pool0 = pool = newPool(0);
    }

    /* get the descriptor */
    descriptor = uti->descriptor__ptr;

    /* do we have enough space? */
    if (pool->end - pool->free >= descriptor->size) {
        /* good, allocate here */
        ret = (struct GGGGC_Header *) pool->free;
        pool->free += descriptor->size;

        /* set its descriptor (no need for write barrier, as this is generation 0) */
        ret->uti__ptr = uti;
#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
        ret->ggggc_memoryCorruptionCheck = GGGGC_MEMORY_CORRUPTION_VAL;
#endif

        /* and clear the rest (necessary since this goes to the untrusted mutator) */
        memset(ret + 1, 0, descriptor->size * sizeof(size_t) - sizeof(struct GGGGC_Header));

    } else if (pool->next) {
        ggggc_pool0 = pool = pool->next;
        goto retry;

    } else if (force) {
        /* get a new pool */
        pool->next = newPool(0);
        ggggc_pool0 = pool = pool->next;
        goto retry;

    } else {
        /* need to collect, which means we need to actually be a GC-safe function */
        GGC_PUSH_1(uti);
        (void) ggggc_local_push;
        ggggc_collect(0);
        GGC_POP();
        pool = ggggc_pool0;
        goto retry;

    }

    return ret;
}

/* allocate an object in the requested generation >= 0 */
void *ggggc_mallocGen1(struct GGGGC_Descriptor *descriptor, /* descriptor for object */
                       unsigned char gen, /* generation to allocate in */
                       int force /* allocate a new pool instead of collecting, if necessary */
                       ) {
    struct GGGGC_Pool *pool;
    struct GGGGC_Header *ret;

retry:
    /* get our allocation pool */
    if (ggggc_pools[gen]) {
        pool = ggggc_pools[gen];
    } else {
        ggggc_gens[gen] = ggggc_pools[gen] = pool = newPool(gen);
    }

    /* do we have enough space? */
    if (pool->end - pool->free >= descriptor->size) {
        size_t retCard, freeCard;

        /* good, allocate here */
        ret = (struct GGGGC_Header *) pool->free;
        pool->free += descriptor->size;
        retCard = GGGGC_CARD_OF(ret);
        freeCard = GGGGC_CARD_OF(pool->free);

        /* if we passed a card, mark the first object */
        if (retCard != freeCard && pool->free < pool->end)
            pool->firstObject[freeCard] =
                ((size_t) pool->free & GGGGC_CARD_INNER_MASK) / sizeof(size_t);

        /* and clear the next descriptor so that it's clear there's no object
         * there (yet) */
        if (pool->free < pool->end) *pool->free = 0;

    } else if (pool->next) {
        ggggc_pools[gen] = pool = pool->next;
        goto retry;

    } else if (force) {
        /* get a new pool */
        pool->next = newPool(gen);
        ggggc_pools[gen] = pool = pool->next;
        goto retry;

    } else {
        /* failed to allocate */
        return NULL;

    }

    return ret;
}

/* allocate an object */
void *ggggc_malloc(struct GGGGC_UserTypeInfo *uti)
{
    return ggggc_mallocGen0(uti, 0);
}

/* allocate a pointer array (size is in words) */
void *ggggc_mallocPointerArray(size_t sz)
{
    struct GGGGC_Descriptor *descriptor = ggggc_allocateDescriptorPA(sz + 1);
    return ggggc_malloc(&descriptor->uti);
}

/* allocate a data array (size is in words) */
void *ggggc_mallocDataArray(size_t sz)
{
    struct GGGGC_Descriptor *descriptor = ggggc_allocateDescriptorDA(sz + 1);
    return ggggc_malloc(&descriptor->uti);
}

/* allocate a descriptor-descriptor for a descriptor of the given size */
struct GGGGC_Descriptor *ggggc_allocateDescriptorDescriptor(size_t size)
{
    struct GGGGC_Descriptor tmpDescriptor, *ret;
    size_t ddSize;

    /* need one description bit for every word in the object */
    ddSize = GGGGC_WORD_SIZEOF(struct GGGGC_Descriptor) + GGGGC_DESCRIPTOR_WORDS_REQ(size);

    /* check if we already have a descriptor */
    if (ggggc_descriptorDescriptors[ddSize])
        return ggggc_descriptorDescriptors[ddSize];

    /* otherwise, need to allocate one. First lock the space */
    ggc_mutex_lock_raw(&ggggc_descriptorDescriptorsLock);
    if (ggggc_descriptorDescriptors[ddSize]) {
        ggc_mutex_unlock(&ggggc_descriptorDescriptorsLock);
        return ggggc_descriptorDescriptors[ddSize];
    }

    /* now make a temporary descriptor to describe the descriptor descriptor */
    tmpDescriptor.uti.header.uti__ptr = NULL;
    tmpDescriptor.uti.descriptor__ptr = &tmpDescriptor;
    tmpDescriptor.size = ddSize;
    tmpDescriptor.pointers[0] = GGGGC_DESCRIPTOR_DESCRIPTION;

    /* allocate the descriptor descriptor */
    ret = ggggc_mallocGen0(&tmpDescriptor.uti, 1);

    /* make it correct */
    ret->uti.descriptor__ptr = ret;
    ret->size = size;
    ret->pointers[0] = GGGGC_DESCRIPTOR_DESCRIPTION;

    /* put it in the list */
    ggggc_descriptorDescriptors[ddSize] = ret;
    ggc_mutex_unlock(&ggggc_descriptorDescriptorsLock);
    GGC_PUSH_1(ggggc_descriptorDescriptors[ddSize]);
    GGC_GLOBALIZE();

    /* and give it a proper descriptor */
    ret->uti.header.uti__ptr = &ggggc_allocateDescriptorDescriptor(ddSize)->uti;

    return ret;
}

/* allocate a descriptor for an object of the given size in words with the
 * given pointer layout */
struct GGGGC_Descriptor *ggggc_allocateDescriptor(size_t size, size_t pointers)
{
    size_t pointersA[1];
    pointersA[0] = pointers;
    return ggggc_allocateDescriptorL(size, pointersA);
}

/* descriptor allocator when more than one word is required to describe the
 * pointers */
struct GGGGC_Descriptor *ggggc_allocateDescriptorL(size_t size, const size_t *pointers)
{
    struct GGGGC_Descriptor *dd, *ret;
    size_t dPWords, dSize;

    /* the size of the descriptor */
    if (pointers)
        dPWords = GGGGC_DESCRIPTOR_WORDS_REQ(size);
    else
        dPWords = 1;
    dSize = GGGGC_WORD_SIZEOF(struct GGGGC_Descriptor) + dPWords;

    /* get a descriptor-descriptor for the descriptor we're about to allocate */
    dd = ggggc_allocateDescriptorDescriptor(dSize);

    /* use that to allocate the descriptor */
    ret = ggggc_mallocGen0(&dd->uti, 1);
    ret->uti.descriptor__ptr = ret;
    ret->size = size;

    /* and set it up */
    if (pointers) {
        memcpy(ret->pointers, pointers, sizeof(size_t) * dPWords);
        ret->pointers[0] |= 1; /* first word is always the descriptor pointer */
    } else {
        ret->pointers[0] = 0;
    }

    return ret;
}

/* descriptor allocator for pointer arrays */
struct GGGGC_Descriptor *ggggc_allocateDescriptorPA(size_t size)
{
    size_t *pointers;
    size_t dPWords, i;

    /* fill our pointer-words with 1s */
    dPWords = GGGGC_DESCRIPTOR_WORDS_REQ(size);
    pointers = alloca(sizeof(size_t) * dPWords);
    for (i = 0; i < dPWords; i++) pointers[i] = (size_t) -1;

    /* and allocate */
    return ggggc_allocateDescriptorL(size, pointers);
}

/* descriptor allocator for data arrays */
struct GGGGC_Descriptor *ggggc_allocateDescriptorDA(size_t size)
{
    /* and allocate */
    return ggggc_allocateDescriptorL(size, NULL);
}

/* allocate a descriptor from a descriptor slot */
struct GGGGC_UserTypeInfo *ggggc_allocateDescriptorSlot(struct GGGGC_DescriptorSlot *slot)
{
    if (slot->uti) return slot->uti;
    ggc_mutex_lock_raw(&slot->lock);
    if (slot->uti) {
        ggc_mutex_unlock(&slot->lock);
        return slot->uti;
    }

    slot->uti = &ggggc_allocateDescriptor(slot->size, slot->pointers)->uti;

    /* make the slot descriptor a root */
    GGC_PUSH_1(slot->uti);
    GGC_GLOBALIZE();

    ggc_mutex_unlock(&slot->lock);
    return slot->uti;
}

/* and a combined malloc/allocslot */
void *ggggc_mallocSlot(struct GGGGC_DescriptorSlot *slot)
{
    return ggggc_malloc(ggggc_allocateDescriptorSlot(slot));
}
