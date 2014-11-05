#ifndef GGGGC_GC_H
#define GGGGC_GC_H 1

#include <stdlib.h>
#ifdef _WIN32
#include <malloc.h>
#else
#include <alloca.h>
#endif

#include "threads.h"
#include "thread-locals.h"

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
#define GGGGC_WORD_SIZEOF(x) ((sizeof(x) + sizeof(size_t) - 1) / sizeof(size_t))
#define GGGGC_POOL_BYTES ((size_t) 1 << GGGGC_POOL_SIZE)
#define GGGGC_POOL_OUTER_MASK ((size_t) -1 << GGGGC_POOL_SIZE)
#define GGGGC_POOL_INNER_MASK (~GGGGC_POOL_OUTER_MASK)
#define GGGGC_POOL_OF(ptr) ((struct GGGGC_Pool *) ((size_t) (ptr) & GGGGC_POOL_OUTER_MASK))
#define GGGGC_GEN_OF(ptr) (GGGGC_POOL_OF(ptr)->gen)
#define GGGGC_CARD_BYTES ((size_t) 1 << GGGGC_CARD_SIZE)
#define GGGGC_CARD_OUTER_MASK ((size_t) -1 << GGGGC_CARD_SIZE)
#define GGGGC_CARD_INNER_MASK (~GGGGC_CARD_OUTER_MASK)
#define GGGGC_CARDS_PER_POOL ((size_t) 1 << (GGGGC_POOL_SIZE-GGGGC_CARD_SIZE))
#define GGGGC_CARD_OF(ptr) (((size_t) (ptr) & GGGGC_POOL_INNER_MASK) >> GGGGC_CARD_SIZE)
#define GGGGC_BITS_PER_WORD (8*sizeof(size_t))
#define GGGGC_WORDS_PER_POOL (GGGGC_POOL_BYTES/sizeof(size_t))

/* GC pool (forms a list) */
struct GGGGC_Pool {
    /* the next pool in this generation */
    struct GGGGC_Pool *next;

    /* the generation of this pool */
    unsigned char gen;

    /* the current free space and end of the pool */
    size_t *free, *end;

    /* how much survived the last collection */
    size_t survivors;

    /* the remembered set for this pool */
    unsigned char remember[GGGGC_CARDS_PER_POOL];

    /* the locations of objects within the cards */
    unsigned short firstObject[GGGGC_CARDS_PER_POOL];

    /* and the actual content */
    size_t start[1];
};

/* GC header (this shape must be shared by all GC'd objects */
struct GGGGC_Header {
    struct GGGGC_Descriptor *descriptor__ptr;
};

/* GGGGC descriptors are GC objects that describe the shape of other GC objects */
struct GGGGC_Descriptor {
    struct GGGGC_Header header;
    void *userPointer; /* for general use */
    size_t size; /* size of the described object in words */
    size_t pointers[1]; /* location of pointers within the object (as a special
                         * case, if pointers[0]|1==0, this means "no pointers") */
};
#define GGGGC_DESCRIPTOR_DESCRIPTION 0x3 /* first two words are pointers */
#define GGGGC_DESCRIPTOR_WORDS_REQ(sz) (((sz) + GGGGC_BITS_PER_WORD - 1) / GGGGC_BITS_PER_WORD)

/* descriptor slots are global locations where descriptors may eventually be
 * stored */
struct GGGGC_DescriptorSlot {
    ggc_mutex_t lock;
    struct GGGGC_Descriptor *descriptor;
    size_t size;
    size_t pointers;
};

/* pointer stacks are used to assure that pointers on the stack are known */
struct GGGGC_PointerStack {
    struct GGGGC_PointerStack *next;
    size_t size;
    void *pointers[1];
};

/* allocate an object */
void *ggggc_malloc(struct GGGGC_Descriptor *descriptor);

/* allocate a pointer array (size is in words) */
void *ggggc_mallocPointerArray(size_t sz);
#define GGC_NEW_PA(type, size) \
    ((type ## Array) ggggc_mallocPointerArray((size)))

/* allocate a data array (size is in words, macro turns it into elements) */
void *ggggc_mallocDataArray(size_t sz);
#define GGC_NEW_DA(type, size) \
    ((GGC_ ## type ## _Array) ggggc_mallocDataArray(((size)*sizeof(type)+sizeof(size_t)-1)/sizeof(size_t)))

/* allocate a descriptor for an object of the given size in words with the
 * given pointer layout */
struct GGGGC_Descriptor *ggggc_allocateDescriptor(size_t size, size_t pointers);

/* descriptor allocator when more than one word is required to describe the
 * pointers */
struct GGGGC_Descriptor *ggggc_allocateDescriptorL(size_t size, const size_t *pointers);

/* descriptor allocator for pointer arrays */
struct GGGGC_Descriptor *ggggc_allocateDescriptorPA(size_t size);

/* descriptor allocator for data arrays */
struct GGGGC_Descriptor *ggggc_allocateDescriptorDA(size_t size);

/* allocate a descriptor from a descriptor slot */
struct GGGGC_Descriptor *ggggc_allocateDescriptorSlot(struct GGGGC_DescriptorSlot *slot);

/* combined malloc + allocateDescriptorSlot */
void *ggggc_mallocSlot(struct GGGGC_DescriptorSlot *slot);
#define GGC_NEW(type) \
    ((type) ggggc_mallocSlot(&type ## __descriptorSlot))

/* macro for making descriptors of types */
#define GGC_DESCRIPTOR(type, pointers) \
    static struct GGGGC_DescriptorSlot type ## __descriptorSlot = { \
        GGC_MUTEX_INITIALIZER, \
        NULL, \
        (sizeof(struct type ## __struct) + sizeof(size_t) - 1) / sizeof(size_t), \
        ((size_t)0) pointers \
    }
#define GGGGC_OFFSETOF(type, member) \
    ((size_t) &((type) 0)->member ## __ptr / sizeof(size_t))
#define GGC_PTR(type, member) \
    | (1<<GGGGC_OFFSETOF(type, member))

/* macros for defining types */
#define GGC_DA_TYPE(type) \
    typedef struct type ## __ggggc_array *GGC_ ## type ## _Array; \
    struct type ## __ggggc_array { \
        struct GGGGC_Header header; \
        type a[1]; \
    }
#define GGC_PA_TYPE(type) \
    typedef struct type ## __ggggc_array *type ## Array; \
    struct type ## __ggggc_array { \
        struct GGGGC_Header header; \
        type a[1]; \
    }
#define GGC_TYPE(type) \
    typedef struct type ## __struct *type; \
    GGC_PA_TYPE(type); \
    struct type ## __struct { \
        struct GGGGC_Header header;
#define GGC_MDATA(type, name) \
        type name
#define GGC_MPTR(type, name) \
        type name ## __ptr
#define GGC_END_TYPE(type, pointers) \
    }; \
    GGC_DESCRIPTOR(type, pointers)

/* a few simple builtin types */
GGC_DA_TYPE(char);
GGC_DA_TYPE(short);
GGC_DA_TYPE(int);
GGC_DA_TYPE(long);
GGC_DA_TYPE(float);
GGC_DA_TYPE(double);

/* write barrier for pointers (NOTE: double-evaluates object, and there's
 * nothing portable that can be done about that :( ) */
#define GGC_W(object, member, value) do { \
    size_t ggggc_o = (size_t) (object); \
    struct GGGGC_Pool *ggggc_pool = GGGGC_POOL_OF(ggggc_o); \
    if (ggggc_pool->gen) { \
        /* a high-gen object, let's remember it */ \
        ggggc_pool->remember[GGGGC_CARD_OF(ggggc_o)] = 1; \
    } \
    (object)->member ## __ptr = (value); \
} while(0)

/* although pointers don't need a read barrier, the renaming sort of forces one */
#define GGC_R(object, member) ((object)->member ## __ptr)

/* macros to push and pop pointers from the pointer stack */
#include "push.h"
#define GGC_POP() do { \
    ggggc_pointerStack = ggggc_pointerStack->next; \
} while(0)
#ifndef GGGGC_NO_REDEFINE_RETURN
#ifdef return
#warning return redefined, being redefined again by GGGGC. Old definition will be discarded!
#undef return
#endif
#define return \
    if ((ggggc_local_push && (ggggc_pointerStack = ggggc_pointerStack->next)) || 1) return
#endif

/* to handle global variables, GGC_PUSH them then GGC_GLOBALIZE */
void ggggc_globalize(void);
#define GGC_GLOBALIZE() ggggc_globalize()

/* usually malloc/NEW will yield for you, but if you want to explicitly yield
 * to the garbage collector (e.g. if you're in a tight loop that doesn't
 * allocate in a multithreaded program), call this */
void ggggc_yield(void);
#define GGC_YIELD() ggggc_yield()

/* each thread has its own pointer stack, including global references */
extern ggc_thread_local struct GGGGC_PointerStack *ggggc_pointerStack, *ggggc_pointerStackGlobals;

/* in order for our #define return to work, we need a global way of knowing we
 * didn't push any local pointers */
#ifndef GGGGC_NO_REDEFINE_RETURN
static const int ggggc_local_push = 0;
#endif

#endif
