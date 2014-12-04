// This is adapted from a benchmark written by John Ellis and Pete Kovac
// of Post Communications.
// It was modified by Hans Boehm of Silicon Graphics.
// Translated to C++ 30 May 1997 by William D Clinger of Northeastern Univ.
// Translated to C 15 March 2000 by Hans Boehm, now at HP Labs.
//
//      This is no substitute for real applications.  No actual application
//      is likely to behave in exactly this way.  However, this benchmark was
//      designed to be more representative of real applications than other
//      Java GC benchmarks of which we are aware.
//      It attempts to model those properties of allocation requests that
//      are important to current GC techniques.
//      It is designed to be used either to obtain a single overall performance
//      number, or to give a more detailed estimate of how collector
//      performance varies with object lifetimes.  It prints the time
//      required to allocate and collect balanced binary trees of various
//      sizes.  Smaller trees result in shorter object lifetimes.  Each cycle
//      allocates roughly the same amount of memory.
//      Two data structures are kept around during the entire process, so
//      that the measured performance is representative of applications
//      that maintain some live in-memory data.  One of these is a tree
//      containing many pointers.  The other is a large array containing
//      double precision floating point numbers.  Both should be of comparable
//      size.
//
//      The results are only really meaningful together with a specification
//      of how much memory was used.  It is possible to trade memory for
//      better time performance.  This benchmark should be run in a 32 MB
//      heap, though we don't currently know how to enforce that uniformly.
//
//      Unlike the original Ellis and Kovac benchmark, we do not attempt
//      measure pause times.  This facility should eventually be added back
//      in.  There are several reasons for omitting it for now.  The original
//      implementation depended on assumptions about the thread scheduler
//      that don't hold uniformly.  The results really measure both the
//      scheduler and GC.  Pause time measurements tend to not fit well with
//      current benchmark suites.  As far as we know, none of the current
//      commercial Java implementations seriously attempt to minimize GC pause
//      times.

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#  include "ggggc/gc.h"

#ifdef PROFIL
  extern void init_profiling();
  extern dump_profile();
#endif

//  These macros were a quick hack for the Macintosh.
//
//  #define currentTime() clock()
//  #define elapsedTime(x) ((1000*(x))/CLOCKS_PER_SEC)

#define currentTime() stats_rtclock()
#define elapsedTime(x) (x)

/* Get the current time in milliseconds */

unsigned
stats_rtclock( void )
{
  struct timeval t;

  if (gettimeofday( &t, NULL ) == -1)
    return 0;
  return (t.tv_sec * 1000 + t.tv_usec / 1000);
}

static const int kStretchTreeDepth    = 18;      // about 16Mb
static const int kLongLivedTreeDepth  = 16;  // about 4Mb
static const int kArraySize  = 500000;  // about 4Mb
static const int kMinTreeDepth = 4;
static const int kMaxTreeDepth = 16;

GGC_TYPE(Node)
    GGC_MPTR(Node, left);
    GGC_MPTR(Node, right);
    GGC_MDATA(int, i);
    GGC_MDATA(int, j);
GGC_END_TYPE(Node,
    GGC_PTR(Node, left)
    GGC_PTR(Node, right)
    )

#ifdef HOLES
#   define HOLE() GGC_NEW(Node);
#else
#   define HOLE()
#endif

void init_Node(Node me, Node l, Node r) {
    GGC_PUSH_3(me, l, r);
    GGC_WP(me, left, l);
    GGC_WP(me, right, r);
    return;
}

// Nodes used by a tree of a given size
static int TreeSize(int i) {
        return ((1 << (i + 1)) - 1);
}

// Number of iterations to use for a given tree depth
static int NumIters(int i) {
        return 2 * TreeSize(kStretchTreeDepth) / TreeSize(i);
}

// Build tree top down, assigning to older objects.
static void Populate(int iDepth, Node thisNode) {
    Node tmp = NULL;
    GGC_PUSH_2(thisNode, tmp);
        if (iDepth<=0) {
                return;
        } else {
                iDepth--;
                tmp = GGC_NEW(Node);
                  GGC_WP(thisNode, left, tmp); HOLE();
                tmp = GGC_NEW(Node);
                  GGC_WP(thisNode, right, tmp); HOLE();
                Populate (iDepth, GGC_RP(thisNode, left));
                Populate (iDepth, GGC_RP(thisNode, right));
        }
        return;
}

// Build tree bottom-up
static Node MakeTree(int iDepth) {
	Node result = NULL;
        Node left = NULL;
        Node right = NULL;

        GGC_PUSH_3(result, left, right);
        if (iDepth<=0) {
		result = GGC_NEW(Node); HOLE();
	    /* result is implicitly initialized in both cases. */
	    return result;
        } else {
            left = MakeTree(iDepth-1);
            right = MakeTree(iDepth-1);
		result = GGC_NEW(Node); HOLE();
	    init_Node(result, left, right);

	    return result;
        }
}

static void PrintDiagnostics() {
#if 0
        long lFreeMemory = Runtime.getRuntime().freeMemory();
        long lTotalMemory = Runtime.getRuntime().totalMemory();

        System.out.print(" Total memory available="
                         + lTotalMemory + " bytes");
        System.out.println("  Free memory=" + lFreeMemory + " bytes");
#endif
}

static void TimeConstruction(int depth) {
        long    tStart, tFinish;
        int     iNumIters = NumIters(depth);
        Node    tempTree = NULL;
	int 	i;

        GGC_PUSH_1(tempTree);

	printf("Creating %d trees of depth %d\n", iNumIters, depth);
        
        tStart = currentTime();
        for (i = 0; i < iNumIters; ++i) {
                  tempTree = GGC_NEW(Node);
                Populate(depth, tempTree);
                tempTree = 0;
        }
        tFinish = currentTime();
        printf("\tTop down construction took %d msec\n",
               (int) elapsedTime(tFinish - tStart));
             
        tStart = currentTime();
        for (i = 0; i < iNumIters; ++i) {
                tempTree = MakeTree(depth);
                tempTree = 0;
        }
        tFinish = currentTime();
        printf("\tBottom up construction took %d msec\n",
               (int) elapsedTime(tFinish - tStart));

    return;

}

int main() {
        Node    root = NULL;
        Node    longLivedTree = NULL;
        Node    tempTree = NULL;
        long    tStart, tFinish;
        long    tElapsed;
  	int	i, d;
	GGC_double_Array array = NULL;

        GGC_PUSH_4(root, longLivedTree, tempTree, array);

	printf("Garbage Collector Test\n");
 	printf(" Live storage will peak at %d bytes.\n\n",
               (int) (2 * sizeof(Node) * TreeSize(kLongLivedTreeDepth) +
               sizeof(double) * kArraySize));
        printf(" Stretching memory with a binary tree of depth %d\n",
               kStretchTreeDepth);
        PrintDiagnostics();
#	ifdef PROFIL
	    init_profiling();
#	endif
       
        tStart = currentTime();
        
        // Stretch the memory space quickly
        tempTree = MakeTree(kStretchTreeDepth);
        tempTree = 0;

        // Create a long lived object
        printf(" Creating a long-lived binary tree of depth %d\n",
               kLongLivedTreeDepth);
          longLivedTree = GGC_NEW(Node);
        Populate(kLongLivedTreeDepth, longLivedTree);

        // Create long-lived array, filling half of it
	printf(" Creating a long-lived array of %d doubles\n", kArraySize);
            array = GGC_NEW_DA(double, kArraySize);
        for (i = 0; i < kArraySize/2; ++i) {
                double tval = 1.0/i;
                GGC_WAD(array, i, tval);
        }
        PrintDiagnostics();

        for (d = kMinTreeDepth; d <= kMaxTreeDepth; d += 2) {
                TimeConstruction(d);
        }

        if (longLivedTree == 0 || GGC_RAD(array, 1000) != 1.0/1000)
		fprintf(stderr, "Failed\n");
                                // fake reference to LongLivedTree
                                // and array
                                // to keep them from being optimized away

        tFinish = currentTime();
        tElapsed = elapsedTime(tFinish-tStart);
        PrintDiagnostics();
        printf("Completed in %d msec\n", (int) tElapsed);
#	ifdef LIBGC
	  printf("Completed %d collections\n", GC_gc_no);
	  printf("Heap size is %d\n", GC_get_heap_size());
#       endif
#	ifdef PROFIL
	  dump_profile();
#	endif
    
    return 0;
}

