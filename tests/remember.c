#include <stdio.h>
#include <stdlib.h>

#include "ggggc.h"
#include "ggggc_internal.h"

GGC_STRUCT(Test,
    Test next;,
    int val;
);

int main(void)
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
    GGC_PTR_WRITE(old, next, test);

    /* and force a collection */
    fprintf(stderr, "%p %p\n", (void *) old, (void *) test);
    GGGGC_collect(0);
    test = GGC_PTR_READ(old, next);

    /* get another new one */
    test2 = GGC_NEW(Test);
    test2->val = 2;
    fprintf(stderr, "%p %p %p\n", (void *) old, (void *) test, (void *) test2);

    /* assert that all is well */
    printf("%d == 1\n", test->val);

    GGC_POP(1);

    return 0;
}
