/* The Computer Language Shootout Benchmarks
   http://shootout.alioth.debian.org/

   contributed by Kevin Carson
   compilation:
       gcc -O3 -fomit-frame-pointer -funroll-loops -static binary-trees.c -lm
       icc -O3 -ip -unroll -static binary-trees.c -lm
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "ggggc/gc.h"

GGC_TYPE(treeNode)
    GGC_MPTR(treeNode, left);
    GGC_MPTR(treeNode, right);
    GGC_MDATA(long, item);
GGC_END_TYPE(treeNode,
    GGC_PTR(treeNode, left)
    GGC_PTR(treeNode, right)
    )

treeNode NewTreeNode(treeNode left, treeNode right, long item)
{
    treeNode    newT = NULL;

    GGC_PUSH_3(left, right, newT);

    newT = GGC_NEW(treeNode);

    GGC_WP(newT, left, left);
    GGC_WP(newT, right, right);
    GGC_WD(newT, item, item);

    return newT;
} /* NewTreeNode() */


long ItemCheck(treeNode tree)
{
    GGC_PUSH_1(tree);
    if (GGC_RP(tree, left) == NULL) {
        return GGC_RD(tree, item);
    } else {
        return GGC_RD(tree, item) + ItemCheck(GGC_RP(tree, left)) - ItemCheck(GGC_RP(tree, right));
    }
} /* ItemCheck() */


treeNode TopDownTree(long item, unsigned depth)
{
    if (depth > 0) {
        treeNode ret, l, r;
        ret = l = r = NULL;
        GGC_PUSH_3(ret, l, r);

        ret = NewTreeNode(NULL, NULL, item);
        l = TopDownTree(2 * item - 1, depth - 1);
        r = TopDownTree(2 * item, depth - 1);
        GGC_WP(ret, left, l);
        GGC_WP(ret, right, r);

        return ret;
    } else
        return NewTreeNode(NULL, NULL, item);
} /* BottomUpTree() */


int main(int argc, char* argv[])
{
    unsigned   N, depth, minDepth, maxDepth, stretchDepth;
    treeNode   stretchTree, longLivedTree, tempTree;

    N = atol(argv[1]);

    minDepth = 4;

    if ((minDepth + 2) > N)
        maxDepth = minDepth + 2;
    else
        maxDepth = N;

    stretchDepth = maxDepth + 1;

    tempTree = stretchTree = longLivedTree = NULL;
    GGC_PUSH_3(tempTree, stretchTree, longLivedTree);

    stretchTree = TopDownTree(0, stretchDepth);
    printf
    (
        "stretch tree of depth %u\t check: %li\n",
        stretchDepth,
        ItemCheck(stretchTree)
    );

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

    return 0;
} /* main() */
