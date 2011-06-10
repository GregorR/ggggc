/*
 * The public GGGGC header
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

#ifndef GGGGC_H
#define GGGGC_H

#include <stdlib.h>

/* These can only be changed if they're changed while compiling, so be careful! */
#ifndef GGGGC_GENERATIONS
#define GGGGC_GENERATIONS 2
#endif

#ifndef GGGGC_POOL_SIZE
#define GGGGC_POOL_SIZE 24 /* pool size as a power of 2 */
#endif

#ifndef GGGGC_CARD_SIZE
#define GGGGC_CARD_SIZE 8 /* also a power of 2 */
#endif

#ifndef GGGGC_PSTACK_SIZE
#define GGGGC_PSTACK_SIZE 256 /* # elements */
#endif

#ifndef GGGGC_HEURISTIC_MAX
#define GGGGC_HEURISTIC_MAX (((size_t) 1 << GGGGC_POOL_SIZE) * 15 / 16)
#endif

/* Various sizes and masks */
#define GGGGC_POOL_BYTES ((size_t) 1 << GGGGC_POOL_SIZE)
#define GGGGC_NOPOOL_MASK ((size_t) -1 << GGGGC_POOL_SIZE)
#define GGGGC_POOL_MASK (~GGGGC_NOPOOL_MASK)
#define GGGGC_CARD_BYTES ((size_t) 1 << GGGGC_CARD_SIZE)
#define GGGGC_NOCARD_MASK ((size_t) -1 << GGGGC_CARD_SIZE)
#define GGGGC_CARD_MASK (~GGGGC_NOCARD_MASK)
#define GGGGC_CARDS_PER_POOL ((size_t) 1 << (GGGGC_POOL_SIZE-GGGGC_CARD_SIZE))
#define GGGGC_CARD_OF(ptr) (((size_t) (ptr) & GGGGC_POOL_MASK) >> GGGGC_CARD_SIZE)

#ifdef GGGGC_DEBUG
#ifndef GGGGC_DEBUG_MEMORY_CORRUPTION
#define GGGGC_DEBUG_MEMORY_CORRUPTION
#endif
#endif

#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
#define GGGGC_HEADER_MAGIC 0x0DEFACED
#endif

/* The GGGGC header */
struct GGGGC_Header {
#ifdef GGGGC_DEBUG_MEMORY_CORRUPTION
    size_t magic;
#endif
    size_t sz;
    unsigned char gen;
    unsigned char type; /* not used by GGGGC, free for end-users */
    unsigned short ptrs;
};


/* Use this macro to make a declaration for a traceable struct */
#define GGC_DECL_STRUCT(name) \
struct _GGGGC__ ## name; \
typedef struct _GGGGC__ ## name * name; \
struct _GGGGC_Array__ ## name; \
typedef struct _GGGGC_Array__ ## name * name ## Array

/* Use this macro to make a definition for a traceable struct */
#define GGC_DEFN_STRUCT(name, ptrs, data) \
struct _GGGGC_Ptrs__ ## name { \
    ptrs \
}; \
static const size_t _ggggc_ptrs_ct__ ## name = sizeof(struct _GGGGC_Ptrs__ ## name) / sizeof(void *); \
struct _GGGGC__ ## name { \
    struct _GGGGC_Ptrs__ ## name _ggggc_ptrs; \
    data \
    void *_ggggc_align; /* not part of the actual allocated memory */ \
}; \
struct _GGGGC_ArrayPtrs__ ## name { \
    name d[1]; \
}; \
struct _GGGGC_Array__ ## name { \
    struct _GGGGC_ArrayPtrs__ ## name _ggggc_ptrs; \
}

/* Use this macro to create a traceable struct. Make sure all GGGGC pointers
 * are in the 'ptrs' part. Due to how C macros work, you can't have comma-lists
 * of elements in these structs, which is really annoying, sorry */
#define GGC_STRUCT(name, ptrs, data) \
GGC_DECL_STRUCT(name); \
GGC_DEFN_STRUCT(name, ptrs, data)

/* Working around some bad C preprocessors, feel free to use this with GGC_DEFN_STRUCT */
#define GGGGC_NOTHING

/* A struct with only pointers */
#define GGC_DEFN_PTR_STRUCT(name, ptrs) GGC_DEFN_STRUCT(name, ptrs, GGGGC_NOTHING)
#define GGC_PTR_STRUCT(name, ptrs) \
GGC_DECL_STRUCT(name); \
GGC_DEFN_PTR_STRUCT(name, ptrs)

/* A struct with only data */
#define GGC_DEFN_DATA_STRUCT(name, data) \
static const size_t _ggggc_ptrs_ct__ ## name = 0; \
struct _GGGGC__ ## name { \
    data \
    void *_ggggc_align; \
}
#define GGC_DATA_STRUCT(name, data) \
GGC_DECL_STRUCT(name); \
GGC_DEFN_DATA_STRUCT(name, data)

/* Try not to use this macro. It will give you the header from any object
 * reference */
#define GGC_HEADER(obj) (((struct GGGGC_Header *) (obj))[-1])

/* Allocate a fresh object of the given type */
#define GGC_NEW(type) ((type) GGGGC_malloc( \
    sizeof(struct _GGGGC__ ## type) + sizeof(struct GGGGC_Header) - sizeof(void *), \
    _ggggc_ptrs_ct__ ## type))
void *GGGGC_malloc(size_t sz, unsigned short ptrs);

/* Allocate an array of the given kind of pointers */
#define GGC_NEW_PTR_ARRAY(type, sz) ((type ## Array) GGGGC_malloc_ptr_array(sz))
#define GGC_NEW_PTR_ARRAY_VOIDP(sz) (GGGGC_malloc_ptr_array(sz))
void *GGGGC_malloc_ptr_array(size_t sz);

/* The ever-unpopular realloc for pointer arrays */
#define GGC_REALLOC_PTR_ARRAY(type, orig, sz) ((type ## Array) GGGGC_realloc_ptr_array((orig), (sz)))
#define GGC_REALLOC_PTR_ARRAY_VOIDP(orig, sz) (GGGGC_realloc_ptr_array((orig), (sz)))
void *GGGGC_realloc_ptr_array(void *orig, size_t sz);

/* Allocate a data array of the given type */
#define GGC_NEW_DATA_ARRAY(type, sz) ((type *) GGGGC_malloc_data_array(sizeof(type) * (sz) + sizeof(struct GGGGC_Header)))
#define GGC_NEW_DATA_ARRAY_VOIDP(type, sz) (GGGGC_malloc_data_array(sizeof(type) * (sz) + sizeof(struct GGGGC_Header)))
void *GGGGC_malloc_data_array(size_t sz);

/* strdup for GGGGC */
#define GGC_STRDUP(str) (strcpy(GGC_NEW_DATA_ARRAY(char, strlen(str) + 1), str))

/* The ever-unpopular realloc for data arrays */
#define GGC_REALLOC_DATA_ARRAY(type, orig, sz) ((type *) GGGGC_realloc_data_array((orig), sizeof(type) * (sz) + sizeof(struct GGGGC_Header)))
#define GGC_REALLOC_DATA_ARRAY_VOIDP(type, orig, sz) (GGGGC_realloc_data_array((orig), sizeof(type) * (sz) + sizeof(struct GGGGC_Header)))
void *GGGGC_realloc_data_array(void *orig, size_t sz);

/* Yield for possible garbage collection (do this frequently) */
#if defined(GGGGC_DEBUG) || defined(GGGGC_FREQUENT_COLLECTIONS)
#define GGC_YIELD() GGGGC_collect(0)
#else
#define GGC_YIELD() do { \
    if (ggggc_heurpool->top > ggggc_heurpoolmax) { \
        GGGGC_collect(0); \
    } \
} while (0)
#endif
void GGGGC_collect(unsigned char gen);

/* Every time you enter a function with pointers in the stack, you MUST push
 * those pointers. Also use this (BEFORE any temporaries) to push global
 * pointers */
#include "ggggcpush.h"
#define GGC_PUSH GGC_PUSH1
void GGGGC_pstackExpand(size_t by);

/* And when you leave the function, remove them */
#define GGC_POP(ct) GGC_YIELD(); ggggc_pstack->rem += (ct); ggggc_pstack->cur -= (ct)

 /* This is used to determine whether a pointer relationship needs to be added
  * to the remembered set */
#define GGGGC_REMEMBER(_from) do { \
    struct GGGGC_Header *_fromhdr = (struct GGGGC_Header *) (_from) - 1; \
    if (_fromhdr->gen > 0) { \
        struct GGGGC_Pool *_pool = (struct GGGGC_Pool *) ((size_t) (_fromhdr) & GGGGC_NOPOOL_MASK); \
        _pool->remember[GGGGC_CARD_OF(_fromhdr)] = 1; \
    } \
} while (0)

/* The write barrier (for pointers) */
#define GGC_PTR_WRITE(_obj, _ptr, _val) do { \
    size_t _sobj = (size_t) (_obj); \
    GGGGC_REMEMBER(_sobj); \
    (_obj)->_ggggc_ptrs._ptr = (void *) (_val); \
} while (0)

/* There is no read barrier, but since ptrs are hidden, use this */
#define GGC_PTR_READ(_obj, _ptr) ((_obj)->_ggggc_ptrs._ptr)

/* Initialize GGGGC */
#define GGC_INIT() GGGGC_init();
void GGGGC_init();


/* The following is mostly internal, but needed for public macros */

/* A GGGGC pool (header) */
struct GGGGC_Pool {
    /* at the beginning to simplify GGGGC_REMEMBER's math */
    char remember[GGGGC_CARDS_PER_POOL];
    struct GGGGC_Pool *next;
    char *top;
    char firstobj[GGGGC_CARDS_PER_POOL];
};

extern struct GGGGC_Pool *ggggc_gens[GGGGC_GENERATIONS+1];
extern struct GGGGC_Pool *ggggc_heurpool, *ggggc_allocpool;
extern char *ggggc_heurpoolmax;

/* The pointer stack */
struct GGGGC_PStack {
    size_t rem;
    void ***cur;
    void **ptrs[1];
};
extern struct GGGGC_PStack *ggggc_pstack;

#endif
