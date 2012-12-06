#include <stdio.h>
#include <stdlib.h>

#include "ggggc.h"

GGC_STRUCT(LLL,
    LLL next;,
    int val;
);

#define MAX (1024 * 1024)

LLL buildLLL(int sz)
{
    int i;
    LLL ll0, lll, llc;

    GGC_PUSH3(ll0, lll, llc);

    ll0 = GGC_NEW(LLL);
    ll0->val = 0;
    lll = ll0;

    for (i = 1; i < sz; i++) {
        llc = GGC_NEW(LLL);
        llc->val = i;
        GGC_PTR_WRITE(lll, next, llc);
        lll = llc;
        GGC_YIELD();
    }

    GGC_POP(3);

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

    GGC_PUSH(lll);

    counted = calloc(MAX, sizeof(unsigned char));
    while (lll) {
        counted[lll->val]++;
        if (counted[lll->val] > 1) {
            fprintf(stderr, "ERROR! Encountered %d twice!\n", lll->val);
            exit(1);
        }
        lll = GGC_PTR_READ(lll, next);
    }

    GGC_POP(1);
}

int main(void)
{
    LLL mylll = NULL;

    GGC_INIT();

    GGC_PUSH(mylll);

    mylll = buildLLL(MAX);
#if 0
    dumpLLL(mylll);
#endif
    testLLL(mylll);

    GGC_YIELD();
    GGC_POP(1);

    return 0;
}
