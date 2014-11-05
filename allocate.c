#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "ggggc/gc.h"
#include "ggggc-internals.h"

/* allocate a pool */
static struct GGGGC_Pool *newPool(unsigned char gen)
{
    struct GGGGC_Pool *ret;

    ret = mmap(NULL, GGGGC_POOL_BYTES, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
    if (ret == NULL) {
        /* FIXME: shouldn't just die */
        perror("mmap");
        exit(1);
    }

    /* space reserved, now set it up */
    ret->gen = gen;
    ret->free = ret->start;
    ret->end = (size_t *) ((unsigned char *) ret + GGGGC_POOL_BYTES);

    /* the first object in the first usable card */
    ret->firstObject[GGGGC_CARD_OF(ret->start)] =
        (((size_t) ret->start) & GGGGC_CARD_INNER_MASK) / sizeof(size_t);

    return ret;
}

/* NOTE: there is code duplication between ggggc_mallocGen0 and
 * ggggc_mallocGenN because I can't trust a compiler to inline and optimize for
 * the 0 case */

/* allocate an object in generation 0 */
void *ggggc_mallocGen0(struct GGGGC_Descriptor *descriptor, /* the object to allocate */
                       int force /* allocate a new pool instead of collecting, if necessary */
                       ) {
    struct GGGGC_Pool *pool;
    struct GGGGC_Header *ret;

retry:
    /* get our allocation pool */
    if (ggggc_pool0) {
        pool = ggggc_pool0;
    } else {
        ggggc_gen0 = ggggc_pool0 = newPool(0);
    }

    /* do we have enough space? */
    if (pool->end - pool->free >= descriptor->size) {
        /* good, allocate here */
        ret = (struct GGGGC_Header *) pool->free;
        pool->free += descriptor->size;

        /* set its descriptor (no need for write barrier, as this is generation 0) */
        ret->descriptor__ptr = descriptor;

    } else if (force) {
        /* get a new pool */
        pool->next = newPool(0);
        ggggc_pool0 = pool = pool->next;
        goto retry;

    } else {
        /* need to collect, which means we need to actually be a GC-safe function */
        GGC_PUSH_1(descriptor);
        ggggc_collect(0);
        GGC_POP();
        goto retry;

    }

    return ret;
}

/* allocate an object in the requested generation >= 0 */
void *ggggc_mallocGen1(struct GGGGC_Descriptor *descriptor, /* the object to allocate */
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
        ggggc_gens[gen] = ggggc_pools[gen] = newPool(gen);
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

        /* set its descriptor */
        GGC_W(ret, descriptor, descriptor);

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
void *ggggc_malloc(struct GGGGC_Descriptor *descriptor)
{
    return ggggc_mallocGen0(descriptor, 0);
}

/* allocate a pointer array (size is in words) */
void *ggggc_malloc_ptr_array(size_t sz)
{
    struct GGGGC_Descriptor *descriptor = ggggc_allocateDescriptorPA(sz + 1);
    return ggggc_malloc(descriptor);
}

/* allocate a data array (size is in words) */
void *ggggc_malloc_data_array(size_t sz)
{
    struct GGGGC_Descriptor *descriptor = ggggc_allocateDescriptorDA(sz + 1);
    return ggggc_malloc(descriptor);
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
    ggc_mutex_lock(&ggggc_descriptorDescriptorsLock);
    if (ggggc_descriptorDescriptors[ddSize]) {
        ggc_mutex_unlock(&ggggc_descriptorDescriptorsLock);
        return ggggc_descriptorDescriptors[ddSize];
    }

    /* now make a temporary descriptor to describe the descriptor descriptor */
    tmpDescriptor.header.descriptor__ptr = &tmpDescriptor;
    tmpDescriptor.size = size;
    tmpDescriptor.pointers[0] = GGGGC_DESCRIPTOR_DESCRIPTION;

    /* allocate the descriptor descriptor */
    ret = ggggc_mallocGen0(&tmpDescriptor, 1);

    /* make it correct */
    ret->size = size;
    ret->pointers[0] = GGGGC_DESCRIPTOR_DESCRIPTION;

    /* put it in the list */
    ggggc_descriptorDescriptors[ddSize] = ret;
    ggc_mutex_unlock(&ggggc_descriptorDescriptorsLock);
    GGC_PUSH_1(ggggc_descriptorDescriptors[ddSize]);
    GGC_GLOBALIZE();
    GGC_POP();

    /* and give it a proper descriptor */
    ret->header.descriptor__ptr = ggggc_allocateDescriptorDescriptor(ddSize);

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
    ret = ggggc_mallocGen0(dd, 1);

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
struct GGGGC_Descriptor *ggggc_allocateDescriptorSlot(struct GGGGC_DescriptorSlot *slot)
{
    if (slot->descriptor) return slot->descriptor;
    ggc_mutex_lock(&slot->lock);
    if (slot->descriptor) {
        ggc_mutex_unlock(&slot->lock);
        return slot->descriptor;
    }

    slot->descriptor = ggggc_allocateDescriptor(slot->size, slot->pointers);
    ggc_mutex_unlock(&slot->lock);
    return slot->descriptor;
}

/* and a combined malloc/allocslot */
void *ggggc_mallocSlot(struct GGGGC_DescriptorSlot *slot)
{
    return ggggc_malloc(ggggc_allocateDescriptorSlot(slot));
}
