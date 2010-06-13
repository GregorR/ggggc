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
    LLL *lll, llr;
    lll = malloc(sz * sizeof(LLL));

    for (i = 0; i < sz; i++) {
        lll[i] = NULL;
        GGC_PUSH(lll[i]);
    }

    for (i = 0; i < sz; i++) {
        lll[i] = GGC_NEW(LLL);
        lll[i]->val = i;
        GGC_YIELD();
    }

    for (i = sz - 2; i >= 0; i--) {
        GGC_PTR_WRITE(lll[i], next, lll[i+1]);
        GGC_POP(1);
        GGC_YIELD();
    }
    GGC_POP(1);

    llr = lll[0];
    free(lll);

    return llr;
}

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

int main()
{
    LLL mylll = NULL;

    GGC_INIT();

    GGC_PUSH(mylll);

    mylll = buildLLL(MAX);
    dumpLLL(mylll);

    GGC_YIELD();
    GGC_POP(1);

    return 0;
}
