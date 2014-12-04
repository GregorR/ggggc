#include <stdio.h>
#include <stdlib.h>

#include "ggggc/gc.h"

GGC_TYPE(LLL)
    GGC_MPTR(LLL, next);
    GGC_MDATA(int, val);
GGC_END_TYPE(LLL,
    GGC_PTR(LLL, next)
    )

#define MAX (1024 * 1024)

LLL buildLLL(int sz)
{
    int i;
    LLL ll0, lll, llc;

    GGC_PUSH_3(ll0, lll, llc);

    ll0 = GGC_NEW(LLL);
    GGC_WD(ll0, val, 0);
    lll = ll0;

    for (i = 1; i < sz; i++) {
        llc = GGC_NEW(LLL);
        GGC_WD(llc, val, i);
        GGC_WP(lll, next, llc);
        lll = llc;
        GGC_YIELD();
    }

    return ll0;
}

#if 0
void dumpLLL(LLL lll)
{
next:
    if (lll) {
        printf("%d ", lll->val);
        lll = GGC_PTR_READ(lll, next);
        goto next;
    }
    printf("\n");
}
#endif

void testLLL(LLL lll)
{
    unsigned char *counted;

    GGC_PUSH_1(lll);

    counted = (unsigned char *) calloc(MAX, sizeof(unsigned char));
    while (lll) {
        counted[GGC_RD(lll, val)]++;
        if (counted[GGC_RD(lll, val)] > 1) {
            fprintf(stderr, "ERROR! Encountered %d twice!\n", GGC_RD(lll, val));
            exit(1);
        }
        lll = GGC_RP(lll, next);
    }

    return;
}

int main(void)
{
    LLL mylll = NULL;

    GGC_PUSH_1(mylll);

    mylll = buildLLL(MAX);
#if 0
    dumpLLL(mylll);
#endif
    testLLL(mylll);

    return 0;
}
