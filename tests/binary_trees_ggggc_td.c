/* The Computer Language Shootout Benchmarks
   http://shootout.alioth.debian.org/

   contributed by Kevin Carson
   compilation:
       gcc -O3 -fomit-frame-pointer -funroll-loops -static binary-trees.c -lm
       icc -O3 -ip -unroll -static binary-trees.c -lm
*/

#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "ggggc.h"

GGC_STRUCT(treeNode,
    treeNode left;
    treeNode right;,
    long item;
);


treeNode NewTreeNode(treeNode left, treeNode right, long item)
{
    treeNode    new;

    GGC_PUSH(left);
    GGC_PUSH(right);
    GGC_PUSH(new);

    new = GGC_NEW(treeNode);

    GGC_PTR_WRITE_UNTAGGED_PTR(new, left, left);
    GGC_PTR_WRITE_UNTAGGED_PTR(new, right, right);
    new->item = item;

    GGC_POP(3);

    return new;
} /* NewTreeNode() */


long ItemCheck(treeNode tree)
{
    GGC_PUSH(tree);
    if (GGC_PTR_READ_UNTAGGING_PTR(tree, left) == NULL) {
        GGC_POP(1);
        return tree->item;
    } else {
        GGC_POP(1);
        return tree->item + ItemCheck(GGC_PTR_READ_UNTAGGING_PTR(tree, left)) - ItemCheck(GGC_PTR_READ_UNTAGGING_PTR(tree, right));
    }
} /* ItemCheck() */


treeNode TopDownTree(long item, unsigned depth)
{
    if (depth > 0) {
        treeNode ret, l, r;
        ret = l = r = NULL;
        GGC_PUSH(ret);
        GGC_PUSH(l);
        GGC_PUSH(r);

        ret = NewTreeNode(NULL, NULL, item);
        l = TopDownTree(2 * item - 1, depth - 1);
        r = TopDownTree(2 * item, depth - 1);
        GGC_PTR_WRITE_UNTAGGED_PTR(ret, left, l);
        GGC_PTR_WRITE_UNTAGGED_PTR(ret, right, r);

        GGC_POP(3);

        return ret;
    } else
        return NewTreeNode(NULL, NULL, item);
} /* BottomUpTree() */


int main(int argc, char* argv[])
{
    unsigned   N, depth, minDepth, maxDepth, stretchDepth;
    treeNode   stretchTree, longLivedTree, tempTree;

    GGC_INIT();

    N = atol(argv[1]);

    minDepth = 4;

    if ((minDepth + 2) > N)
        maxDepth = minDepth + 2;
    else
        maxDepth = N;

    stretchDepth = maxDepth + 1;

    tempTree = NULL;
    GGC_PUSH(tempTree);

    stretchTree = NULL;
    GGC_PUSH(stretchTree);

    stretchTree = TopDownTree(0, stretchDepth);
    printf
    (
        "stretch tree of depth %u\t check: %li\n",
        stretchDepth,
        ItemCheck(stretchTree)
    );

    longLivedTree = NULL;
    GGC_PUSH(longLivedTree);
    longLivedTree = TopDownTree(0, maxDepth);

    for (depth = minDepth; depth <= maxDepth; depth += 2)
    {
        long    i, iterations, check;

        iterations = pow(2, maxDepth - depth + minDepth);

        check = 0;

        for (i = 1; i <= iterations; i++)
        {
            tempTree = TopDownTree(i, depth);
            check += ItemCheck(tempTree);

            tempTree = TopDownTree(-i, depth);
            check += ItemCheck(tempTree);
        } /* for(i = 1...) */

        printf
        (
            "%li\t trees of depth %u\t check: %li\n",
            iterations * 2,
            depth,
            check
        );
    } /* for(depth = minDepth...) */

    printf
    (
        "long lived tree of depth %u\t check: %li\n",
        maxDepth,
        ItemCheck(longLivedTree)
    );

    GGC_POP(3);

    return 0;
} /* main() */
