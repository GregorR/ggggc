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

#define U ggggc_height++
#define D ggggc_height--
#define LPSS if (__builtin_expect(ggggc_save, 0)) { if (ggggc_height >= ggggc_restore)
#define LPSR(r) return r; } else if (__builtin_expect(ggggc_height < ggggc_restore, 0))
#define LPSE ggggc_restore--

#define SAVE(ptr) (*ggggc_pstack.cur++ = (void *) ptr)
#define RESTORE(ptr) (ptr = *ggggc_pstack.front++)

GGC_STRUCT(treeNode,
    treeNode left;
    treeNode right;,
    long item;
);


treeNode NewTreeNode(treeNode left, treeNode right, long item)
{
    treeNode    new = NULL;

    new = GGC_NEW(treeNode);

    GGC_PTR_WRITE(new, left, left);
    GGC_PTR_WRITE(new, right, right);
    new->item = item;

    GGC_YIELD();
    LPSS { SAVE(new); }
    LPSR(NULL) { RESTORE(new); LPSE; }

    return new;
} /* NewTreeNode() */


long ItemCheck(treeNode tree)
{
    if (GGC_PTR_READ(tree, left) == NULL) {
        return tree->item;
    } else {
        long l, r;

        U; l = ItemCheck(GGC_PTR_READ(tree, left)); D;
        LPSS { SAVE(tree); }
        LPSR(0) { RESTORE(tree); LPSE; }

        U; r = ItemCheck(GGC_PTR_READ(tree, right)); D;
        LPSS { SAVE(tree); }
        LPSR(0) { RESTORE(tree); LPSE; }

        GGC_YIELD();
        LPSS { SAVE(tree); }
        LPSR(0) { RESTORE(tree); LPSE; }

        return tree->item + l - r;
    }
} /* ItemCheck() */


treeNode TopDownTree(long item, unsigned depth)
{
    if (depth > 0) {
        treeNode ret, l, r;
        ret = l = r = NULL;

        U; ret = NewTreeNode(NULL, NULL, item); D;
        LPSS { SAVE(l); SAVE(r); }
        LPSR(NULL) { RESTORE(l); RESTORE(r); LPSE; }

        U; l = TopDownTree(2 * item - 1, depth - 1); D;
        LPSS { SAVE(ret); SAVE(r); }
        LPSR(NULL) { RESTORE(ret); RESTORE(r); LPSE; }

        U; r = TopDownTree(2 * item, depth - 1); D;
        LPSS { SAVE(ret); SAVE(l); }
        LPSR(NULL) { RESTORE(ret); RESTORE(l); LPSE; }

        GGC_PTR_WRITE(ret, left, l);
        GGC_PTR_WRITE(ret, right, r);

        GGC_YIELD();
        LPSS { SAVE(ret); SAVE(l); SAVE(r); }
        LPSR(NULL) { RESTORE(ret); RESTORE(l); RESTORE(r); LPSE; }

        return ret;
    } else
        return NewTreeNode(NULL, NULL, item);
} /* BottomUpTree() */


int realmain(int argc, char* argv[])
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

    U; stretchTree = TopDownTree(0, stretchDepth); D;
    LPSS { SAVE(tempTree); SAVE(longLivedTree); }
    LPSR(0) { RESTORE(tempTree); RESTORE(longLivedTree); LPSE; }

    {
        long ic;
        U; ic = ItemCheck(stretchTree); D;
        LPSS { SAVE(tempTree); SAVE(stretchTree); SAVE(longLivedTree); }
        LPSR(0) { RESTORE(tempTree); RESTORE(stretchTree); RESTORE(longLivedTree); LPSE; }
        printf
        (
            "stretch tree of depth %u\t check: %li\n",
            stretchDepth, ic
        );
    }

    U; longLivedTree = TopDownTree(0, maxDepth); D;
    LPSS { SAVE(tempTree); SAVE(stretchTree); }
    LPSR(0) { RESTORE(tempTree); RESTORE(stretchTree); LPSE; }

    for (depth = minDepth; depth <= maxDepth; depth += 2)
    {
        long    i, iterations, check;

        iterations = pow(2, maxDepth - depth + minDepth);

        check = 0;

        for (i = 1; i <= iterations; i++)
        {
            U; tempTree = TopDownTree(i, depth); D;
            LPSS { SAVE(stretchTree); SAVE(longLivedTree); }
            LPSR(0) { RESTORE(stretchTree); RESTORE(longLivedTree); LPSE; }

            U; check += ItemCheck(tempTree); D;
            LPSS { SAVE(tempTree); SAVE(stretchTree); SAVE(longLivedTree); }
            LPSR(0) { RESTORE(tempTree); RESTORE(stretchTree); RESTORE(longLivedTree); LPSE; }

            U; tempTree = TopDownTree(-i, depth); D;
            LPSS { SAVE(stretchTree); SAVE(longLivedTree); }
            LPSR(0) { RESTORE(stretchTree); RESTORE(longLivedTree); LPSE; }

            U; check += ItemCheck(tempTree); D;
            LPSS { SAVE(tempTree); SAVE(stretchTree); SAVE(longLivedTree); }
            LPSR(0) { RESTORE(tempTree); RESTORE(stretchTree); RESTORE(longLivedTree); LPSE; }

        } /* for(i = 1...) */

        printf
        (
            "%li\t trees of depth %u\t check: %li\n",
            iterations * 2,
            depth,
            check
        );
    } /* for(depth = minDepth...) */

    {
        long ic;
        U; ic = ItemCheck(longLivedTree); D;
        LPSS { SAVE(tempTree); SAVE(stretchTree); SAVE(longLivedTree); }
        LPSR(0) { RESTORE(tempTree); RESTORE(stretchTree); RESTORE(longLivedTree); LPSE; }

        printf
        (
            "long lived tree of depth %u\t check: %li\n",
            maxDepth, ic
        );
    }

    GGC_YIELD();
    LPSS { SAVE(tempTree); SAVE(stretchTree); SAVE(longLivedTree); }
    LPSR(0) { RESTORE(tempTree); RESTORE(stretchTree); RESTORE(longLivedTree); LPSE; }

    return 0;
} /* main() */

int deadspace(int argc, char **argv)
{
    alloca(4096);
    return realmain(argc, argv);
}

int main(int argc, char **argv)
{
    int ret;

    GGC_INIT();

    ret = deadspace(argc, argv);
    if (ggggc_save) {
        GGGGC_restoreAndZip();
    }
    return ret;
}
