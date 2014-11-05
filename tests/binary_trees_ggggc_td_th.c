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

GGC_DA_TYPE(ggc_thread_t)

treeNode NewTreeNode(treeNode left, treeNode right, long item)
{
    treeNode    new = NULL;

    GGC_PUSH_3(left, right, new);

    new = GGC_NEW(treeNode);

    GGC_W(new, left, left);
    GGC_W(new, right, right);
    new->item = item;

    return new;
} /* NewTreeNode() */


long ItemCheck(treeNode tree)
{
    GGC_PUSH_1(tree);
    if (GGC_R(tree, left) == NULL) {
        return tree->item;
    } else {
        return tree->item + ItemCheck(GGC_R(tree, left)) - ItemCheck(GGC_R(tree, right));
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
        GGC_W(ret, left, l);
        GGC_W(ret, right, r);

        return ret;
    } else
        return NewTreeNode(NULL, NULL, item);
} /* BottomUpTree() */


void treeThread(ThreadArg arg)
{
        unsigned minDepth, depth, maxDepth;
        long    i, iterations, check;
        treeNode tempTree = NULL;
        GGC_unsigned_Array aa = NULL;

        GGC_PUSH_3(arg, tempTree, aa);

        aa = GGC_R(arg, parg);
        minDepth = aa->a[0];
        depth = aa->a[1];
        maxDepth = aa->a[2];

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

    return;
}


int main(int argc, char* argv[])
{
    unsigned   N, depth, minDepth, maxDepth, stretchDepth;
    treeNode   stretchTree, longLivedTree, tempTree;
    GGC_ggc_thread_t_Array threads;
    GGC_unsigned_Array targs;
    ThreadArg targ;

    N = atol(argv[1]);

    minDepth = 4;

    if ((minDepth + 2) > N)
        maxDepth = minDepth + 2;
    else
        maxDepth = N;

    stretchDepth = maxDepth + 1;

    tempTree = stretchTree = longLivedTree = NULL;
    threads = NULL;
    targs = NULL;
    targ = NULL;
    GGC_PUSH_6(tempTree, stretchTree, longLivedTree, threads, targs, targ);

    stretchTree = TopDownTree(0, stretchDepth);
    printf
    (
        "stretch tree of depth %u\t check: %li\n",
        stretchDepth,
        ItemCheck(stretchTree)
    );

    longLivedTree = TopDownTree(0, maxDepth);

    threads = GGC_NEW_DA(ggc_thread_t, maxDepth + 1);

    for (depth = minDepth; depth <= maxDepth; depth += 2)
    {
        ggc_thread_t th;

        /* set up our arguments */
        targs = GGC_NEW_DA(unsigned, 3);
        targs->a[0] = minDepth;
        targs->a[1] = depth;
        targs->a[2] = maxDepth;
        targ = GGC_NEW(ThreadArg);
        GGC_W(targ, parg, targs);

        /* and create the thread */
        ggc_thread_create(&th, treeThread, targ);
        threads->a[depth] = th;
    } /* for(depth = minDepth...) */

    /* wait for all the threads */
    for (depth = minDepth; depth <= maxDepth; depth += 2)
    {
        ggc_thread_join(threads->a[depth]);
    }

    printf
    (
        "long lived tree of depth %u\t check: %li\n",
        maxDepth,
        ItemCheck(longLivedTree)
    );

    return 0;
} /* main() */
