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
#endif

/* The GGGGC header */
struct GGGGC_Header {
    size_t sz;
    unsigned char ptrs;
};

/* Create a traceable struct */
#define GGC_STRUCT(name, ptrs, data) \
struct _GGGGC__ ## name; \
typedef struct _GGGGC__ ## name * name; \
struct _GGGGC_Ptrs_ ## name { \
    ptrs \
}; \
struct _GGGGC__ ## name { \
    struct GGGGC_Header _ggggc_header; \
    struct _GGGGC_Ptrs_ ## name _ggggc_ptrs; \
    data \
}

/* Allocate a fresh object of the given type */
#define GGC_ALLOC(type) (type *) GGGGC_malloc(sizeof(struct _GGGGC__ ## type), sizeof(_GGGGC_Ptrs_ ## type) / sizeof(void *))
void *GGGGC_malloc(size_t sz, unsigned char ptrs);

/* Add this pointer to the "roots" list (used to avoid needing a typed stack generally) */
#define GGC_PUSH(obj) GGGGC_push(&(obj))
void GGGGC_push(void **ptr);

/* Remove a pointer from the "roots" list, MUST strictly be a pop */
#define GGC_POP() GGGGC_pop()
void GGGGC_pop();

/* Yield for possible garbage collection (do frequently) */
#define GGC_YIELD() do { \
    if (ggggc_gens[0]->top - ggggc_gens[0] > GGGGC_GENERATION_BYTES * 3 / 4) { \
        GGGGC_collect(0); \
    } \
} while (0)
void GGGGC_collect(char gen);

/* Write to a pointer */
#define GGC_PTR_WRITE(_obj, _ptr, _val) do { \
    size_t _sobj = (size_t) (_obj); \
    struct GGGGC_Generation *_gen = (struct GGGGC_Generation *) (_sobj & ((size_t) -1 << GGGGC_GENERATION_SIZE)); \
    _gen->remember[_sobj & ~((size_t) -1 << GGGGC_CARD_SIZE)] = 1; \
    (_obj)->_ggggc_ptrs. ## _ptr = (_val); \
} while (0)

/* Read from a pointer */
#define GGC_PTR_READ(_obj, _ptr) ((_obj)->_ggggc_ptrs. ## _ptr)

/* A GGGGC generation (header) */
struct GGGGC_Generation {
    char *top;
    char remember[1];
};
extern struct GGGGC_Generation *ggggc_gens[GGGGC_GENERATIONS];

/* Initialize GGGGC */
#define GGC_INIT() GGGGC_init();
void GGGGC_init();

#endif
