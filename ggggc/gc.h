/*
 * GGGGC: Gregor's General Generational Garbage Collector
 *
 * Copyright (c) 2014, 2015 Gregor Richards
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

#ifndef GGGGC_GC_H
#define GGGGC_GC_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <sys/types.h>
#ifdef _WIN32
#include <malloc.h>
#else
#include <alloca.h>
#endif

#include "thread-locals.h"
#include "threads.h"

/* debugging flags */
#ifdef GGGGC_DEBUG
#define GGGGC_DEBUG_MEMORY_CORRUPTION 1
#define GGGGC_DEBUG_REPORT_COLLECTIONS 1
#define GGGGC_DEBUG_TINY_HEAP 1
#endif

/* flags to disable GCC features */
#ifdef GGGGC_NO_GNUC_FEATURES
#define GGGGC_NO_GNUC_CLEANUP 1
#define GGGGC_NO_GNUC_CONSTRUCTOR 1
#endif

/* word-sized integer type, usually size_t */
#ifndef ggc_size_t
typedef size_t ggc_size_t;
#endif

#ifndef GGGGC_GENERATIONS
#define GGGGC_GENERATIONS 2
#endif

#ifndef GGGGC_POOL_SIZE
#define GGGGC_POOL_SIZE 24 /* pool size as a power of 2 */
#endif

#ifndef GGGGC_CARD_SIZE
#define GGGGC_CARD_SIZE 12 /* also a power of 2 */
#endif

/* various sizes and masks */
#define GGGGC_WORD_SIZEOF(x) ((sizeof(x) + sizeof(ggc_size_t) - 1) / sizeof(ggc_size_t))
#define GGGGC_POOL_BYTES ((ggc_size_t) 1 << GGGGC_POOL_SIZE)
#define GGGGC_POOL_OUTER_MASK ((ggc_size_t) -1 << GGGGC_POOL_SIZE)
#define GGGGC_POOL_INNER_MASK (~GGGGC_POOL_OUTER_MASK)
#define GGGGC_POOL_OF(ptr) ((struct GGGGC_Pool *) ((ggc_size_t) (ptr) & GGGGC_POOL_OUTER_MASK))
#define GGGGC_GEN_OF(ptr) (GGGGC_POOL_OF(ptr)->gen)
#define GGGGC_CARD_BYTES ((ggc_size_t) 1 << GGGGC_CARD_SIZE)
#define GGGGC_CARD_OUTER_MASK ((ggc_size_t) -1 << GGGGC_CARD_SIZE)
#define GGGGC_CARD_INNER_MASK (~GGGGC_CARD_OUTER_MASK)
#define GGGGC_CARDS_PER_POOL ((ggc_size_t) 1 << (GGGGC_POOL_SIZE-GGGGC_CARD_SIZE))
#define GGGGC_CARD_OF(ptr) (((ggc_size_t) (ptr) & GGGGC_POOL_INNER_MASK) >> GGGGC_CARD_SIZE)
#define GGGGC_BITS_PER_WORD (8*sizeof(ggc_size_t))
#define GGGGC_WORDS_PER_POOL (GGGGC_POOL_BYTES/sizeof(ggc_size_t))

/* an empty defined for all the various conditions in which empty defines are necessary */
#define GGGGC_EMPTY

/* GC pool (forms a list) */
struct GGGGC_Pool {
#if GGGGC_GENERATIONS > 1
    /* the remembered set for this pool. NOTE: It's important this be first to
     * make assigning to the remembered set take one less operation */
    unsigned char remember[GGGGC_CARDS_PER_POOL];

    /* the locations of objects within the cards */
    unsigned short firstObject[GGGGC_CARDS_PER_POOL];
#endif

    /* the next pool in this generation */
    struct GGGGC_Pool *next;

    /* the generation of this pool */
    unsigned char gen;

    /* the current free space and end of the pool */
    ggc_size_t *free, *end;

    /* how much survived the last collection */
    ggc_size_t survivors;

    /* size of the break table (in entries, used only during collection) */
    ggc_size_t breakTableSize;

    /* pointer to the break table (used only during collection) */
    void *breakTable;

    /* and the actual content */
    ggc_size_t start[1];
};

#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
#define GGGGC_MEMORY_CORRUPTION_VAL 0x0DEFACED
#endif

/* GC header (this shape must be shared by all GC'd objects) */
struct GGGGC_Header {
    struct GGGGC_Descriptor *descriptor__ptr;
#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
    ggc_size_t ggggc_memoryCorruptionCheck;
#endif
};

/* GGGGC descriptors are GC objects that describe the shape of other GC objects */
struct GGGGC_Descriptor {
    struct GGGGC_Header header;
    void *user__ptr; /* for the user to use however they please */
    ggc_size_t size; /* size of the described object in words */
    ggc_size_t pointers[1]; /* location of pointers within the object (as a special
                         * case, if pointers[0]&1==0, this means "no pointers") */
};
#define GGGGC_DESCRIPTOR_DESCRIPTION (((ggc_size_t)1<<(((ggc_size_t) (void *) &((struct GGGGC_Header *) 0)->descriptor__ptr)/sizeof(ggc_size_t)))|\
                                      ((ggc_size_t)1<<(((ggc_size_t) (void *) &((struct GGGGC_Descriptor *) 0)->user__ptr)/sizeof(ggc_size_t)))) 
#define GGGGC_DESCRIPTOR_WORDS_REQ(sz) (((sz) + GGGGC_BITS_PER_WORD - 1) / GGGGC_BITS_PER_WORD)

/* descriptor slots are global locations where descriptors may eventually be
 * stored */
struct GGGGC_DescriptorSlot {
    ggc_mutex_t lock;
    struct GGGGC_Descriptor *descriptor;
    ggc_size_t size;
    ggc_size_t pointers;
};

/* pointer stacks are used to assure that pointers on the stack are known */
struct GGGGC_PointerStack {
    struct GGGGC_PointerStack *next;
    ggc_size_t size;
    void *pointers[1];
};

/* macro for making descriptors of types */
#if defined(__GNUC__) && !defined(GGGGC_NO_GNUC_CONSTRUCTOR)
#define GGGGC_DESCRIPTORS_CONSTRUCTED
#define GGGGC_DESCRIPTOR_CONSTRUCTOR(type) \
static void __attribute__((constructor)) type ## __descriptorSlotConstructor() { \
    ggggc_allocateDescriptorSlot(&type ## __descriptorSlot); \
}

#elif defined(__cplusplus)
#define GGGGC_DESCRIPTORS_CONSTRUCTED
#define GGGGC_DESCRIPTOR_CONSTRUCTOR(type) \
class type ## __descriptorSlotConstructor { \
    public: \
    type ## __descriptorSlotConstructor() { \
        ggggc_allocateDescriptorSlot(&type ## __descriptorSlot); \
    } \
}; \
static type ## __descriptorSlotConstructor type ## __descriptorSlotConstructorInstance; \

#else
#define GGGGC_DESCRIPTOR_CONSTRUCTOR(type)

#endif

#define GGC_DESCRIPTOR(type, pointers) \
    static struct GGGGC_DescriptorSlot type ## __descriptorSlot = { \
        GGC_MUTEX_INITIALIZER, \
        NULL, \
        (sizeof(struct type ## __ggggc_struct) + sizeof(ggc_size_t) - 1) / sizeof(ggc_size_t), \
        ((ggc_size_t)0) pointers \
    }; \
    GGGGC_DESCRIPTOR_CONSTRUCTOR(type)
#define GGGGC_OFFSETOF(type, member) \
    ((ggc_size_t) &((type) 0)->member ## __ptr / sizeof(ggc_size_t))
#define GGC_PTR(type, member) \
    | (1<<GGGGC_OFFSETOF(type, member))
#define GGC_NO_PTRS | 0

/* macros for defining types 
 * Example:
 * GGC_TYPE(Foo)
 *     GGC_MPTR(Bar, fooMemberOfTypeBar);
 *     GGC_MDATA(int, fooMemberOfTypeInt);
 * GGC_END_TYPE(Foo,
 *     GGC_PTR(Foo, fooMemberOfTypeBar)
 *     )
 */
#define GGC_DA_TYPE(type) \
    typedef struct type ## __ggggc_darray *GGC_ ## type ## _Array; \
    struct type ## __ggggc_darray { \
        struct GGGGC_Header header; \
        ggc_size_t length; \
        type a__data[1]; \
    };
#define GGC_PA_TYPE(type) \
    typedef struct type ## __ggggc_parray *type ## Array; \
    struct type ## __ggggc_parray { \
        struct GGGGC_Header header; \
        ggc_size_t length; \
        type a__ptrs[1]; \
    };
#define GGC_TYPE(type) \
    typedef struct type ## __ggggc_struct *type; \
    GGC_PA_TYPE(type) \
    struct type ## __ggggc_struct { \
        struct GGGGC_Header header;
#define GGC_MDATA(type, name) \
        type name ## __data
#define GGC_MPTR(type, name) \
        type name ## __ptr
#define GGC_END_TYPE(type, pointers) \
    }; \
    GGC_DESCRIPTOR(type, pointers)

#define GGGGC_ASSERT_ID(thing) do { \
    void *thing ## _must_be_an_identifier; \
    (void) thing ## _must_be_an_identifier; \
} while(0)

/* write barriers */
#if GGGGC_GENERATIONS > 1
#define GGGGC_WP(object, member, value) do { \
    ggc_size_t ggggc_o = (ggc_size_t) (object); \
    struct GGGGC_Pool *ggggc_pool = GGGGC_POOL_OF(ggggc_o); \
    GGGGC_ASSERT_ID(object); \
    GGGGC_ASSERT_ID(value); \
    if (ggggc_pool->gen) { \
        /* a high-gen object, let's remember it */ \
        ggggc_pool->remember[GGGGC_CARD_OF(ggggc_o)] = 1; \
    } \
    (object)->member = (value); \
} while(0)
#else
#define GGGGC_WP(object, member, value) do { \
    GGGGC_ASSERT_ID(object); \
    GGGGC_ASSERT_ID(value); \
    (object)->member = (value); \
} while(0)
#endif
#define GGGGC_WD(object, member, value) do { \
    GGGGC_ASSERT_ID(object); \
    GGGGC_ASSERT_ID(if_not_a_value_then_ ## value); \
    (object)->member = (value); \
} while(0)

/* write a normal member */
#define GGC_WP(object, member, value) GGGGC_WP(object, member ## __ptr, value)
#define GGC_WD(object, member, value) GGGGC_WD(object, member ## __data, value)

/* write an array member */
#define GGC_WAP(object, index, value) GGGGC_WP(object, a__ptrs[(index)], value)
#define GGC_WAD(object, index, value) GGGGC_WD(object, a__data[(index)], value)

/* write the descriptor user pointer */
#define GGC_WUP(object, value) do { \
    struct GGGGC_Descriptor *ggggc_desc = (object)->header.descriptor__ptr; \
    GGGGC_ASSERT_ID(object); \
    GGGGC_WP(ggggc_desc, user__ptr, value); \
} while(0)

/* although pointers don't need a read barrier, the renaming sort of forces one */
#define GGC_RP(object, member)  ((object)->member ## __ptr)
#define GGC_RD(object, member)  ((object)->member ## __data)
#define GGC_RAP(object, index)  ((object)->a__ptrs[(index)])
#define GGC_RAD(object, index)  ((object)->a__data[(index)])
#define GGC_RUP(object)         ((object)->header.descriptor__ptr->user__ptr)
#define GGC_LENGTH(object)      ((object)->header.descriptor__ptr->length)

/* because the write barrier forces you to use identifiers, an identifier version of NULL */
static void * const ggggc_null = NULL;
#define GGC_NULL ggggc_null

/* allocate an object */
void *ggggc_malloc(struct GGGGC_Descriptor *descriptor);

/* combined malloc + allocateDescriptorSlot */
void *ggggc_mallocSlot(struct GGGGC_DescriptorSlot *slot);

/* general allocator */
#ifdef GGGGC_DESCRIPTORS_CONSTRUCTED
#define GGC_NEW(type) ((type) ggggc_malloc(type ## __descriptorSlot.descriptor))
#else
#define GGC_NEW(type) ((type) ggggc_mallocSlot(&type ## __descriptorSlot))
#endif

/* allocate a pointer array (size is in words) */
void *ggggc_mallocPointerArray(ggc_size_t sz);
#define GGC_NEW_PA(type, size) \
    ((type ## Array) ggggc_mallocPointerArray((size)))

/* allocate a data array (size is in words, macro turns it into elements) */
void *ggggc_mallocDataArray(ggc_size_t nmemb, ggc_size_t size);
#define GGC_NEW_DA(type, size) \
    ((GGC_ ## type ## _Array) ggggc_mallocDataArray((size), sizeof(type)))

/* allocate a descriptor for an object of the given size in words with the
 * given pointer layout */
struct GGGGC_Descriptor *ggggc_allocateDescriptor(ggc_size_t size, ggc_size_t pointers);

/* descriptor allocator when more than one word is required to describe the
 * pointers */
struct GGGGC_Descriptor *ggggc_allocateDescriptorL(ggc_size_t size, const ggc_size_t *pointers);

/* descriptor allocator for pointer arrays */
struct GGGGC_Descriptor *ggggc_allocateDescriptorPA(ggc_size_t size);

/* descriptor allocator for data arrays */
struct GGGGC_Descriptor *ggggc_allocateDescriptorDA(ggc_size_t size);

/* allocate a descriptor from a descriptor slot */
struct GGGGC_Descriptor *ggggc_allocateDescriptorSlot(struct GGGGC_DescriptorSlot *slot);

/* global heuristic for "please stop the world" */
extern volatile int ggggc_stopTheWorld;

/* usually malloc/NEW and return will yield for you, but if you want to
 * explicitly yield to the garbage collector (e.g. if you're in a tight loop
 * that doesn't allocate in a multithreaded program), call this */
int ggggc_yield(void);
#define GGC_YIELD() (ggggc_stopTheWorld ? ggggc_yield() : 0)

/* to handle global variables, GGC_PUSH them then GGC_GLOBALIZE */
void ggggc_globalize(void);
#define GGC_GLOBALIZE() ggggc_globalize()

/* each thread has its own pointer stack, including global references */
extern ggc_thread_local struct GGGGC_PointerStack *ggggc_pointerStack, *ggggc_pointerStackGlobals;

/* macros to push and pop pointers from the pointer stack */
#define GGGGC_POP() do { \
    ggggc_pointerStack = ggggc_pointerStack->next; \
} while(0)

#if defined(__GNUC__) && !defined(GGGGC_NO_GNUC_CLEANUP)
static inline void ggggc_pop(void *i) {
    GGGGC_POP();
}
#define GGGGC_LOCAL_PUSH void * __attribute__((cleanup(ggggc_pop))) __attribute__((unused)) ggggc_localPush = NULL;
#define GGC_POP() do {} while(0)

#elif defined(__cplusplus)
} /* end extern "C" */

class GGGGC_LocalPush {
    public:
    ~GGGGC_LocalPush() {
        GGGGC_POP();
    }
};
#define GGGGC_LOCAL_PUSH GGGGC_LocalPush ggggc_localPush;
#define GGC_POP() do {} while(0)

extern "C" {

#else
/* we have to be hacky to approximate this for other compilers */
static const int ggggc_localPush = 0;
#define GGGGC_LOCAL_PUSH const int ggggc_localPush = 1;
#define GGC_POP() GGGGC_POP()

#ifdef return
#warning return redefined, being redefined again by GGGGC. Old definition will be discarded!
#undef return
#endif
#define return \
    if (ggggc_localPush ? \
        ((ggggc_pointerStack = ggggc_pointerStack->next), 0) : \
        0) {} else return

#endif

#include "push.h"

/* the type passed to threads, which allows both GC and non-GC args */
GGC_TYPE(ThreadArg)
    GGC_MPTR(void *, parg);
    GGC_MDATA(void *, darg);
GGC_END_TYPE(ThreadArg,
    GGC_PTR(ThreadArg, parg)
    )

/* support for ggggcify tool */
#ifdef GGGGC_GGGGCIFY
#include "ggggcify.h"
#endif

/* a few simple builtin types */
GGC_DA_TYPE(char)
GGC_DA_TYPE(short)
GGC_DA_TYPE(int)
GGC_DA_TYPE(unsigned)
GGC_DA_TYPE(long)
GGC_DA_TYPE(float)
GGC_DA_TYPE(double)
GGC_DA_TYPE(size_t)

typedef void *GGC_voidp;
GGC_PA_TYPE(GGC_voidp)

#ifdef __cplusplus
}
#endif

#endif
