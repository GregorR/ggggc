// This is adapted from a benchmark written by John Ellis and Pete Kovac
// of Post Communications.
// It was modified by Hans Boehm of Silicon Graphics.
// Translated to C++ 30 May 1997 by William D Clinger of Northeastern Univ.
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

#ifndef THREADS
#  define BOOST_DISABLE_THREADS
#else
#  define _REENTRANT
#  include <pthread.h>
#  define BOOST_LWM_USE_SPINLOCK
#endif
#include <new>
#include <iostream>
#include <sys/time.h>
#include <boost/intrusive_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/detail/quick_allocator.hpp>
#include <boost/detail/lightweight_mutex.hpp>

using boost::intrusive_ptr;
using boost::shared_array;
using boost::detail::lightweight_mutex;

//  These macros were a quick hack for the Macintosh.
//
//#define currentTime() clock()
//#define elapsedTime(x) ((1000*(x))/CLOCKS_PER_SEC)

// #include <windows.h>
// #include <mmsystem.h>

// #define currentTime() timeGetTime()
// #define elapsedTime(x) (x)


#define currentTime() stats_rtclock()
#define elapsedTime(x) (x)

/* Get the current time in milliseconds */
unsigned
stats_rtclock( void )
{
  struct timeval t;
  struct timezone tz;

  if (gettimeofday( &t, &tz ) == -1)
    return 0;
  return (t.tv_sec * 1000 + t.tv_usec / 1000);
}
static const int kStretchTreeDepth    = 18;      // about 16Mb
static const int kLongLivedTreeDepth  = 16;  // about 4Mb
static const int kArraySize  = 500000;  // about 4Mb
static const int kMinTreeDepth = 4;
static const int kMaxTreeDepth = 16;

struct Node0 {
    intrusive_ptr<Node0> left, right;
    int i, j;

    int refs;
#   ifdef THREADS
      lightweight_mutex mutex;
#   endif

    Node0(intrusive_ptr<Node0> const & l, intrusive_ptr<Node0> const & r):
	  left(l), right(r), refs(0) {}
    Node0(): refs(0) {}

    typedef Node0 this_type;

#   ifdef CUSTOM_ALLOCATOR
      void * operator new(std::size_t)
      {
	return boost::detail::quick_allocator<this_type>::alloc();
      }

      void operator delete(void * p)
      {
        boost::detail::quick_allocator<this_type>::dealloc(p);
      }
#   endif

};

typedef intrusive_ptr<Node0> Node;

inline void intrusive_ptr_add_ref(Node0 * p)
{
#ifdef THREADS
        lightweight_mutex::scoped_lock lock(p->mutex);
#endif
	++p->refs;
}

inline void intrusive_ptr_release(Node0 * p)
{
   int count;
   {
#ifdef THREADS
     lightweight_mutex::scoped_lock lock(p->mutex);
#endif
     count = --p->refs;
   }
   if(count == 0) delete p;
}

struct GCBench {

        // Nodes used by a tree of a given size
        static int TreeSize(int i) {
                return ((1 << (i + 1)) - 1);
        }

        // Number of iterations to use for a given tree depth
        static int NumIters(int i) {
                return 2 * TreeSize(kStretchTreeDepth) / TreeSize(i);
        }

        // Build tree top down, assigning to older objects.
        static void Populate(int iDepth, Node const & thisNode) {
                if (iDepth<=0) {
                        return;
                } else {
                        iDepth--;
                        thisNode->left = Node(new Node0());
                        thisNode->right = Node(new Node0());
                        Populate (iDepth, thisNode->left);
                        Populate (iDepth, thisNode->right);
                }
        }

        // Build tree bottom-up
        static Node MakeTree(int iDepth) {
                if (iDepth<=0) {
                     return Node(new Node0());
                } else {
                     return Node(new Node0(MakeTree(iDepth-1),
                                                 MakeTree(iDepth-1)));
                }
        }

        static void PrintDiagnostics() {
        }

#	ifdef TIME_DESTR
#	  define DESTR_START() tStart_destr = currentTime();
#	  define DESTR_END() \
		tFinish_destr = currentTime(); \
		if (tFinish_destr - tStart_destr > tMax_destr) { \
		  tMax_destr = tFinish_destr - tStart_destr; \
		}
#	else
#	  define DESTR_START()
#	  define DESTR_END()
#	endif

        static void TimeConstruction(int depth) {
                long    tStart, tFinish;
#		ifdef TIME_DESTR
                  long    tStart_destr, tFinish_destr;
		  long	  tMax_destr = 0;
#		endif
                int     iNumIters = NumIters(depth);
                Node    tempTree;

		std::cout << "Creating " << iNumIters
                     << " trees of depth " << depth << std::endl;
                
                tStart = currentTime();
                for (int i = 0; i < iNumIters; ++i) {
                        Node tempTree(new Node0());
                        Populate(depth, tempTree);
			DESTR_START();
                        tempTree = Node();
			DESTR_END();
                }
                tFinish = currentTime();
		std::cout << "\tTop down construction took "
                     << elapsedTime(tFinish - tStart) << " msec" << std::endl;
#		ifdef TIME_DESTR
		  std::cout << "\tMax top down destruction time "
                     << elapsedTime(tMax_destr) << " msec" << std::endl;
		  tMax_destr = 0;
#		endif
                     
                tStart = currentTime();
                for (int i = 0; i < iNumIters; ++i) {
                        tempTree = MakeTree(depth);
			DESTR_START();
                        tempTree = Node();
			DESTR_END();
                }
                tFinish = currentTime();
		std::cout << "\tBottom up construction took "
                     << elapsedTime(tFinish - tStart) << " msec" << std::endl;
#		ifdef TIME_DESTR
		  std::cout << "\tMax bottom up destruction time "
                     << elapsedTime(tMax_destr) << " msec" << std::endl;
		  tMax_destr = 0;
#		endif

        }

static void *noop(void *x) { return x; }

        void main() {
                Node    root;
                Node    longLivedTree;
                Node    tempTree;
                long    tStart, tFinish;
                long    tElapsed;
		char * startHeap = (char *)sbrk(0);

#      		ifdef THREADS
		  {
		    pthread_t thr;
		    pthread_attr_t attr;
		    pthread_attr_init(&attr);
  	            pthread_create(&thr, &attr, noop, (void *)0);
		  }
#		endif

		std::cout << "Garbage Collector Test" << std::endl;
		std::cout << " Live storage will peak at "
                     << sizeof(Node0) * TreeSize(kLongLivedTreeDepth) +
		        sizeof(Node0) * TreeSize(kMaxTreeDepth) +
                        sizeof(double) * kArraySize
                     << " bytes." << std::endl << std::endl;
		std::cout << " Stretching memory with a binary tree of depth "
                     << kStretchTreeDepth << std::endl;
                PrintDiagnostics();
               
                tStart = currentTime();
                
                // Stretch the memory space quickly
                tempTree = MakeTree(kStretchTreeDepth);
                tempTree = Node();

                // Create a long lived object
		std::cout << " Creating a long-lived binary tree of depth "
                     << kLongLivedTreeDepth << std::endl;
                longLivedTree = Node(new Node0());
                Populate(kLongLivedTreeDepth, longLivedTree);

                // Create long-lived array, filling half of it
		std::cout << " Creating a long-lived array of "
                     << kArraySize << " doubles" << std::endl;
                shared_array<double> array(new double[kArraySize]);
                for (int i = 0; i < kArraySize/2; ++i) {
                        array[i] = 1.0/i;
                }
                PrintDiagnostics();

                for (int d = kMinTreeDepth; d <= kMaxTreeDepth; d += 2)
{
                        TimeConstruction(d);
                }

                if (longLivedTree == 0 || array[1000] != 1.0/1000)
                        std::cout << "Failed" << std::endl;
                                        // fake reference to LongLivedTree
                                        // and array
                                        // to keep them from being optimized away

                tFinish = currentTime();
                tElapsed = elapsedTime(tFinish-tStart);
                PrintDiagnostics();
		std::cout << "Completed in " << tElapsed << " msec"
			  << std::endl;
		std::cout << "Heap size is " << (char *)sbrk(0) - startHeap
		          << std::endl;
        }
};

int main () {
//	timeBeginPeriod(1);
    GCBench x;
    x.main();
//	timeEndPeriod(1);
}

