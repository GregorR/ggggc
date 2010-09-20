#include <stdio.h>
#include <stdlib.h>

#include "ggggc.h"
#include "ggggc_internal.h"

GGC_STRUCT(Test,
    Test next;,
    int val;
);

int main()
{
    Test old;
    Test test, test2;

    GGC_INIT();

    GGC_PUSH(old);

    /* allocate our "old" one */
    old = (Test) GGGGC_trymalloc_gen(1, 1, sizeof(struct _GGGGC__Test), 1);
    old->val = 0;

    /* allocate but do not push */
    test = GGC_NEW(Test);
    test->val = 1;

    /* now point to it from the old one */
    GGC_PTR_WRITE_UNTAGGED_PTR(old, next, test);

    /* and force a collection */
    fprintf(stderr, "%p %p\n", old, test);
    GGGGC_collect(0);
    test = GGC_PTR_READ_UNTAGGING_PTR(old, next);

    /* get another new one */
    test2 = GGC_NEW(Test);
    test2->val = 2;
    fprintf(stderr, "%p %p %p\n", old, test, test2);

    /* assert that all is well */
    printf("%d == 1\n", test->val);

    GGC_POP(1);

    return 0;
}
