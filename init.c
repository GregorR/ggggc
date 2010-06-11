#define _BSD_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "ggggc.h"
#include "helpers.h"

static void *allocateAligned(size_t sz2)
{
    void *ret;
    size_t sz = 1<<sz2;

    /* mmap double */
    SF(ret, mmap, NULL, (NULL, sz*2, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0));

    /* check for alignment */
    if (((size_t) ret & ~((size_t) -1 << sz2)) != 0) {
        /* not aligned, figure out the proper alignment */
        void *base = (void *) (((size_t) ret + sz) & ((size_t) -1 << sz2));
        munmap(ret, (char *) base - (char *) ret);
        munmap((void *) ((char *) base + sz), sz - ((char *) base - (char *) ret));
        ret = base;
    } else {
        /* aligned, just free the excess */
        munmap((void *) ((char *) ret + sz), sz);
    }

    return ret;
}

void GGGGC_init()
{
    int g, cc;
    for (g = 0; g < GGGGC_GENERATIONS; g++) {
        /* allocate this generation */
        struct GGGGC_Generation *gen = (struct GGGGC_Generation *) allocateAligned(GGGGC_GENERATION_SIZE);

        /* clear out the cards */
        cc = 1 << (GGGGC_GENERATION_SIZE - GGGGC_CARD_SIZE);
        memset(gen->remember, 0, cc);

        /* set up the top pointer */
        gen->top = (void *) (gen->remember + cc);
    }
}
