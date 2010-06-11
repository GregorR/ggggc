#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ggggc.h"
#include "helpers.h"

static void *GGGGC_trymalloc_gen(unsigned char gen, size_t sz, unsigned char ptrs)
{
    if (gen < GGGGC_GENERATIONS) {
        struct GGGGC_Generation *ggen = ggggc_gens[gen];
        if (ggen->top + sz <= ((char *) ggen) + (1<<GGGGC_GENERATION_SIZE)) {
            /* sufficient room, just give it */
            struct GGGGC_Header *ret = (struct GGGGC_Header *) ggen->top;
            ggen->top += sz;
            ret->sz = sz;
            ret->ptrs = ptrs;
            memset(ret + 1, 0, ptrs * sizeof(void *));
            return ret;
        }

        /* or try a higher gen */
        return GGGGC_trymalloc_gen(gen, sz, ptrs);

    } else {
        /* if we fall off, just have to leak */
        void *ret;
        SF(ret, malloc, NULL, (sz));
        return ret;

    }
}

void *GGGGC_malloc(size_t sz, unsigned char ptrs)
{
    return GGGGC_trymalloc_gen(0, sz, ptrs);
}
