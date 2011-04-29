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

unsigned maxDepth, minDepth;


treeNode NewTreeNode(treeNode left, treeNode right, long item)
{
    treeNode    new = NULL;

    GGC_PUSH3(new, left, right);

    new = GGC_NEW(treeNode);

    GGC_PTR_WRITE(new, left, left);
    GGC_PTR_WRITE(new, right, right);
    new->item = item;

    GGC_POP(3);
    return new;
} /* NewTreeNode() */


long ItemCheck(treeNode tree)
{
    GGC_PUSH(tree);
    if (GGC_PTR_READ(tree, left) == NULL) {
        GGC_POP(1);
        return tree->item;
    } else {
        GGC_POP(1);
        return tree->item + ItemCheck(GGC_PTR_READ(tree, left)) - ItemCheck(GGC_PTR_READ(tree, right));
    }
} /* ItemCheck() */


treeNode BottomUpTree(long item, unsigned depth)
{
    if (depth > 0) {
        treeNode ret, l, r;
        ret = l = r = NULL;
        GGC_PUSH3(ret, l, r);

        l = BottomUpTree(2 * item - 1, depth - 1);
        r = BottomUpTree(2 * item, depth - 1);
        ret = NewTreeNode(l, r, item);

        GGC_POP(3);

        return ret;
    } else
        return NewTreeNode(NULL, NULL, item);
} /* BottomUpTree() */


void *someTree(void *depthvp)
{
    unsigned depth = (unsigned) (size_t) depthvp;
        long    i, iterations, check;
    treeNode   stretchTree, longLivedTree, tempTree;

    GGC_PUSH3(stretchTree, longLivedTree, tempTree);

        iterations = pow(2, maxDepth - depth + minDepth);

        check = 0;

        for (i = 1; i <= iterations; i++)
        {
            tempTree = BottomUpTree(i, depth);
            check += ItemCheck(tempTree);

            tempTree = BottomUpTree(-i, depth);
            check += ItemCheck(tempTree);
        } /* for(i = 1...) */

        printf
        (
            "%li\t trees of depth %u\t check: %li\n",
            iterations * 2,
            depth,
            check
        );

    GGC_POP(3);
}

int main(int argc, char* argv[])
{
    unsigned   N, depth, stretchDepth;
    treeNode   stretchTree, longLivedTree, tempTree;

    GGC_INIT();
    stretchTree = longLivedTree = tempTree = NULL;
    GGC_PUSH3(stretchTree, longLivedTree, tempTree);

    N = atol(argv[1]);

    minDepth = 4;

    if ((minDepth + 2) > N)
        maxDepth = minDepth + 2;
    else
        maxDepth = N;

    stretchDepth = maxDepth + 1;

    stretchTree = BottomUpTree(0, stretchDepth);
    printf
    (
        "stretch tree of depth %u\t check: %li\n",
        stretchDepth,
        ItemCheck(stretchTree)
    );

    longLivedTree = BottomUpTree(0, maxDepth);

    for (depth = minDepth; depth <= maxDepth; depth += 2)
    {
        GGC_thread_create(NULL, someTree, (void *) (size_t) depth);
    } /* for(depth = minDepth...) */
    someTree((void *) (size_t) maxDepth);
    someTree((void *) (size_t) maxDepth);
    someTree((void *) (size_t) maxDepth);
    someTree((void *) (size_t) maxDepth);
    someTree((void *) (size_t) maxDepth);

    printf
    (
        "long lived tree of depth %u\t check: %li\n",
        maxDepth,
        ItemCheck(longLivedTree)
    );

    GGC_POP(3);

    return 0;
} /* main() */
