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
#define GGGGC_POOL_SIZE 27 /* pool size as a power of 2 */
#endif

#ifndef GGGGC_CARD_SIZE
#define GGGGC_CARD_SIZE 7 /* also a power of 2 */
#endif

/* Various sizes and masks */
#define GGGGC_POOL_BYTES (1<<GGGGC_POOL_SIZE)
#define GGGGC_NOPOOL_MASK ((size_t) -1 << GGGGC_POOL_SIZE)
#define GGGGC_POOL_MASK (~GGGGC_NOPOOL_MASK)
#define GGGGC_CARD_BYTES (1<<GGGGC_CARD_SIZE)
#define GGGGC_NOCARD_MASK ((size_t ) -1 << GGGGC_CARD_SIZE)
#define GGGGC_CARD_MASK (~GGGGC_NOCARD_MASK)
#define GGGGC_CARDS_PER_POOL (1<<(GGGGC_POOL_SIZE-GGGGC_CARD_SIZE))
#define GGGGC_CARD_OF(ptr) (((size_t) (ptr) & GGGGC_POOL_MASK) >> GGGGC_CARD_SIZE)

/* The GGGGC header */
struct GGGGC_Header {
    size_t sz;
    unsigned char gen, ptrs;
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
    struct GGGGC_Header _ggggc_header; \
    ptrs \
}; \
struct _GGGGC__ ## name { \
    struct _GGGGC_Ptrs__ ## name _ggggc_ptrs; \
    data \
}; \
struct _GGGGC_PtrsArray__ ## name { \
    struct GGGGC_Header _ggggc_header; \
    struct _GGGGC__ ## name d[1]; \
}; \
struct _GGGGC_Array__ ## name { \
    struct _GGGGC_PtrsArray__ ## name _ggggc_ptrs; \
}

/* Use this macro to create a traceable struct. Make sure all GGGGC pointers
 * are in the 'ptrs' part. Due to how C macros work, you can't have comma-lists
 * of elements in these structs, which is really annoying, sorry */
#define GGC_STRUCT(name, ptrs, data) \
GGC_DECL_STRUCT(name); \
GGC_DEFN_STRUCT(name, ptrs, data)

/* Working around some bad C preprocessors */
#define GGGGC_NOTHING

/* A struct with only pointers */
#define GGC_PTR_STRUCT(name, ptrs) GGC_STRUCT(name, ptrs, GGGGC_NOTHING)

/* A struct with only data */
#define GGC_DATA_STRUCT(name, data) GGC_STRUCT(name, GGGGC_NOTHING, data)

/* Use this macro to define a data array; that is, an array without pointers.
 * GGC_STRUCT will define pointer arrays automatically */
#define GGC_DATA_ARRAY(name, type) \
typedef struct _GGGGC__ ## name ## Array * name ## Array; \
struct _GGGGC_Ptrs__ ## name ## Array { \
    struct GGGGC_Header _ggggc_header; \
}; \
struct _GGGGC_Elem__ ## name ## Array { \
    type d; \
}; \
struct _GGGGC__ ## name ## Array { \
    struct _GGGGC_Ptrs__ ## name ## Array _ggggc_ptrs; \
    type d[1]; \
}

/* Some common data arrays */
GGC_DATA_ARRAY(char, char);
GGC_DATA_ARRAY(int, int);

/* Allocate a fresh object of the given type */
#define GGC_NEW(type) ((type) GGGGC_malloc(sizeof(struct _GGGGC__ ## type), \
    (sizeof(struct _GGGGC_Ptrs__ ## type) - sizeof(struct GGGGC_Header)) / sizeof(void *)))
void *GGGGC_malloc(size_t sz, unsigned char ptrs);

/* Allocate an array of the given kind of pointers */
#define GGC_NEW_PTR_ARRAY(type, sz) ((type ## Array) GGGGC_malloc_ptr_array(sizeof(type), (sz)))
void *GGGGC_malloc_ptr_array(size_t sz, size_t nmemb);

/* Allocate an array of the given NAME (not type) */
#define GGC_NEW_DATA_ARRAY(name, sz) ((name ## Array) GGGGC_malloc_data_array( \
    sizeof(struct _GGGGC_Elem__ ## name ## Array), (sz)))
void *GGGGC_malloc_data_array(size_t sz, size_t nmemb);

/* Yield for possible garbage collection (do this frequently) */
#define GGC_YIELD() do { \
    if (ggggc_heurpool->remaining <= GGGGC_POOL_BYTES / 10) { \
        GGGGC_collect(0); \
    } \
} while (0)
void GGGGC_collect(unsigned char gen);

/* Every time you enter a function with pointers in the stack, you MUST push
 * those pointers. Also use this (BEFORE any temporaries) to push global
 * pointers */
#define GGC_PUSH(obj) GGGGC_push((void **) &(obj))
#define GGC_PUSH2(obj, obj2) GGGGC_push2((void **) &(obj), (void **) &(obj2))
#define GGC_PUSH3(obj, obj2, obj3) GGGGC_push3((void **) &(obj), (void **) &(obj2), (void **) &(obj3))
#define GGC_PUSH4(obj, obj2, obj3, obj4) GGGGC_push4((void **) &(obj), (void **) &(obj2), (void **) &(obj3), (void **) &(obj4))
#define GGC_PUSH5(obj, obj2, obj3, obj4, obj5) GGGGC_push5((void **) &(obj), (void **) &(obj2), (void **) &(obj3), (void **) &(obj4), (void **) &(obj5))
#define GGC_PUSH6(obj, obj2, obj3, obj4, obj5, obj6) GGGGC_push6((void **) &(obj), (void **) &(obj2), (void **) &(obj3), (void **) &(obj4), (void **) &(obj5), (void **) &(obj6))
#define GGC_PUSH7(obj, obj2, obj3, obj4, obj5, obj6, obj7) GGGGC_push7((void **) &(obj), (void **) &(obj2), (void **) &(obj3), (void **) &(obj4), (void **) &(obj5), (void **) &(obj6), (void **) &(obj7))
#define GGC_PUSH8(obj, obj2, obj3, obj4, obj5, obj6, obj7, obj8) GGGGC_push8((void **) &(obj), (void **) &(obj2), (void **) &(obj3), (void **) &(obj4), (void **) &(obj5), (void **) &(obj6), (void **) &(obj7), (void **) &(obj8))
#define GGC_PUSH9(obj, obj2, obj3, obj4, obj5, obj6, obj7, obj8, obj9) GGGGC_push9((void **) &(obj), (void **) &(obj2), (void **) &(obj3), (void **) &(obj4), (void **) &(obj5), (void **) &(obj6), (void **) &(obj7), (void **) &(obj8), (void **) &(obj9))
void GGGGC_push(void **);
void GGGGC_push2(void **, void **);
void GGGGC_push3(void **, void **, void **);
void GGGGC_push4(void **, void **, void **, void **);
void GGGGC_push5(void **, void **, void **, void **, void **);
void GGGGC_push6(void **, void **, void **, void **, void **, void **);
void GGGGC_push7(void **, void **, void **, void **, void **, void **, void **);
void GGGGC_push8(void **, void **, void **, void **, void **, void **, void **, void **);
void GGGGC_push9(void **, void **, void **, void **, void **, void **, void **, void **, void **);

/* And when you leave the function, remove them */
#define GGC_POP(ct) GGC_YIELD(); GGGGC_pop(ct)
void GGGGC_pop(int ct);

/* The write barrier (for pointers) */
#define GGC_PTR_WRITE(_obj, _ptr, _val) do { \
    size_t _sobj = (size_t) (_obj); \
    struct GGGGC_Header *_gval = (void *) (_val); \
    if ((_gval) && (_obj)->_ggggc_ptrs._ggggc_header.gen > (_gval)->gen) { \
        struct GGGGC_Pool *_pool = (struct GGGGC_Pool *) (_sobj & GGGGC_NOPOOL_MASK); \
        _pool->remember[(_sobj & GGGGC_POOL_MASK) >> GGGGC_CARD_SIZE] = 1; \
    } \
    (_obj)->_ggggc_ptrs._ptr = (void *) (_gval); \
} while (0)

/* There is no read barrier, but since ptrs are hidden, use this */
#define GGC_PTR_READ(_obj, _ptr) ((_obj)->_ggggc_ptrs._ptr)

/* Initialize GGGGC */
#define GGC_INIT() GGGGC_init();
void GGGGC_init();


/* The following is mostly internal, but needed for public macros */

/* A GGGGC pool (header) */
struct GGGGC_Pool {
    char *top;
    size_t remaining; /* bytes remaining in the pool */
    char remember[GGGGC_CARDS_PER_POOL];
    char firstobj[GGGGC_CARDS_PER_POOL];
};

/* A GGGGC generation, which is several pools (header) */
struct GGGGC_Generation {
    size_t poolc;
    struct GGGGC_Pool *pools[1];
};
extern struct GGGGC_Generation *ggggc_gens[GGGGC_GENERATIONS+1];
extern struct GGGGC_Pool *ggggc_heurpool, *ggggc_allocpool;

#endif
