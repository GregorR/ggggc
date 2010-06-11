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

#ifndef GGGGC_GENERATIONS
#define GGGGC_GENERATIONS 2
#endif

#ifndef GGGGC_GENERATION_SIZE
#define GGGGC_GENERATION_SIZE 27 /* generation size as a power of 2 */
#define GGGGC_GENERATION_BYTES (1<<GGGGC_GENERATION_SIZE)
#endif

#ifndef GGGGC_CARD_SIZE
#define GGGGC_CARD_SIZE 7 /* also a power of 2 */
#define GGGGC_CARD_BYTES (1<<GGGGC_CARD_SIZE)
#endif

#define GGGGC_CARDS_PER_GENERATION (1<<(GGGGC_GENERATION_SIZE-GGGGC_CARD_SIZE))

/* The GGGGC header */
struct GGGGC_Header {
    size_t sz;
    unsigned char gen, ptrs;
};

/* Create a traceable struct */
#define GGC_STRUCT(name, ptrs, data) \
struct _GGGGC__ ## name; \
typedef struct _GGGGC__ ## name * name; \
struct _GGGGC_Ptrs__ ## name { \
    struct GGGGC_Header _ggggc_header; \
    ptrs \
}; \
struct _GGGGC__ ## name { \
    struct _GGGGC_Ptrs__ ## name _ggggc_ptrs; \
    data \
}; \
struct _GGGGC_Array__ ## name; \
typedef struct _GGGGC_Array__ ## name * name ## Array; \
struct _GGGGC_PtrsArray__ ## name { \
    struct GGGGC_Header _ggggc_header; \
    struct _GGGGC__ ## name d[1]; \
}; \
struct _GGGGC_Array__ ## name { \
    struct _GGGGC_PtrsArray__ ## name _ggggc_ptrs; \
}

#define NOTHING

/* A struct with only pointers */
#define GGC_PTR_STRUCT(name, ptrs) GGC_STRUCT(name, ptrs, NOTHING)

/* A struct with only data */
#define GGC_DATA_STRUCT(name, data) GGC_STRUCT(name, NOTHING, data)

/* Allocate a fresh object of the given type */
#define GGC_ALLOC(type) ((type) GGGGC_malloc(sizeof(struct _GGGGC__ ## type), \
    (sizeof(struct _GGGGC_Ptrs__ ## type) - sizeof(struct GGGGC_Header)) / sizeof(void *)))
void *GGGGC_malloc(size_t sz, unsigned char ptrs);

/* Allocate an array of the given kind of pointers */
#define GGC_ALLOC_PTR_ARRAY(type, sz) ((type ## Array) GGGGC_malloc_array(sizeof(type), (sz)))
void *GGGGC_malloc_array(size_t sz, size_t nmemb);

/* Allocate an array of the given kind of data */
#define GGC_ALLOC_DATA_ARRAY(type, sz) ((type *) GGGGC_malloc((sz) * sizeof(type), 0))

/* Add this pointer to the "roots" list (used to avoid needing a typed stack generally) */
#define GGC_PUSH(obj) GGGGC_push((void **) &(obj))
void GGGGC_push(void **ptr);

/* Remove pointers from the "roots" list, MUST strictly be a pop */
#define GGC_POP(ct) GGGGC_pop(ct)
void GGGGC_pop(int ct);

/* Yield for possible garbage collection (do frequently) */
#define GGC_YIELD() do { \
    if (ggggc_gens[0]->top - (char *) ggggc_gens[0] > GGGGC_GENERATION_BYTES * 3 / 4) { \
        GGGGC_collect(0); \
    } \
} while (0)
void GGGGC_collect(unsigned char gen);

/* Write to a pointer */
#define GGC_PTR_WRITE(_obj, _ptr, _val) do { \
    size_t _sobj = (size_t) (_obj); \
    if ((_val) && (_obj)->_ggggc_ptrs._ggggc_header.gen > (_val)->_ggggc_ptrs._ggggc_header.gen) { \
        struct GGGGC_Pool *_pool = (struct GGGGC_Generation *) (_sobj & ((size_t) -1 << GGGGC_GENERATION_SIZE)); \
        _pool->remember[(_sobj & ~((size_t) -1 << GGGGC_GENERATION_SIZE)) >> GGGGC_CARD_SIZE] = 1; \
    } \
    (_obj)->_ggggc_ptrs._ptr = (_val); \
} while (0)
#undef GGC_PTR_WRITE
#define GGC_PTR_WRITE(_obj, _ptr, _val) do { \
    (_obj)->_ggggc_ptrs._ptr = (_val); \
} while (0)


/* Read from a pointer */
#define GGC_PTR_READ(_obj, _ptr) ((_obj)->_ggggc_ptrs._ptr)

/* A GGGGC pool (header) */
struct GGGGC_Pool {
    char *top;
    char remember[GGGGC_CARDS_PER_GENERATION];
    char firstobj[GGGGC_CARDS_PER_GENERATION];
};
extern struct GGGGC_Pool *ggggc_gens[GGGGC_GENERATIONS+1];

/* Initialize GGGGC */
#define GGC_INIT() GGGGC_init();
void GGGGC_init();

#endif
