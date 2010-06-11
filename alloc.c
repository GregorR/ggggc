#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ggggc.h"
#include "helpers.h"

void *GGGGC_trymalloc_gen(unsigned char gen, size_t sz, unsigned char ptrs)
{
    if (gen < GGGGC_GENERATIONS && sz <= (1<<GGGGC_CARD_SIZE)) {
        struct GGGGC_Generation *ggen = ggggc_gens[gen];

        /* we can't allocate on a card boundary */
        size_t c1 = ((size_t) ggen->top) >> GGGGC_CARD_SIZE;
        size_t c2 = ((size_t) ggen->top + sz - 1) >> GGGGC_CARD_SIZE;
        if (c1 != c2) {
            /* quick-allocate the intermediate space */
            size_t isz = (c2 << GGGGC_CARD_SIZE) - (size_t) ggen->top;
            GGGGC_trymalloc_gen(gen, isz, 0);
        }
        if (ggen->top + sz <= ((char *) ggen) + (1<<GGGGC_GENERATION_SIZE)) {
            /* sufficient room, just give it */
            struct GGGGC_Header *ret = (struct GGGGC_Header *) ggen->top;
            ggen->top += sz;
            memset(ret, 0, sz);
            ret->sz = sz;
            ret->gen = gen;
            ret->ptrs = ptrs;
            return ret;
        }

        /* or fail */
        return NULL;

    } else {
        /* if we fall off, can't safely give anything */
        fprintf(stderr, "Memory exhausted.\n");
        *((int *) 0) = 0;
        return NULL;

    }
}

void *GGGGC_malloc(size_t sz, unsigned char ptrs)
{
    int i;
    void *ret;
    for (i = 0; i <= GGGGC_GENERATIONS; i++) {
        ret = GGGGC_trymalloc_gen(i, sz, ptrs);
        if (ret) return ret;
    }

    /* SHOULD NEVER HAPPEN */
    return NULL;
}
