#include <stdio.h>
#include <stdlib.h>

#include "ggggc.h"

GGC_STRUCT(Tree,
    Tree sub[2];,
    int written;
);

static void dumpTree(Tree tree)
{
    int i;
    for (i = 0; i < 2; i++) {
        Tree sub = GGC_PTR_READ(tree, sub[i]);
        printf("%p -> %p;\n", (void *) tree, (void *) sub);
        if (sub && !sub->written) {
            sub->written = 1;
            dumpTree(sub);
        }
    }
}

int main(int argc, char **argv)
{
    Tree trees[4];
    int i, ct, a, b, c;
    ct = atoi(argv[1]);

    GGC_INIT();

    trees[0] = trees[1] = trees[2] = trees[3] = NULL;

    GGC_PUSH(trees[0]);
    GGC_PUSH(trees[1]);
    GGC_PUSH(trees[2]);
    GGC_PUSH(trees[3]);

    trees[0] = GGC_ALLOC(Tree);
    trees[1] = GGC_ALLOC(Tree);
    trees[2] = GGC_ALLOC(Tree);
    trees[3] = GGC_ALLOC(Tree);

    /* now munge up some "trees" */
    for (i = 0; i < ct; i++) {
    }

    printf("digraph {\n");
    dumpTree(trees[0]);
    printf("}\n");

    return 0;
}
