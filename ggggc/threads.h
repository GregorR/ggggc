#ifndef GGGGC_THREADS_H 
#define GGGGC_THREADS_H 1

/* get our feature macros */
#if defined(unix) || defined(__unix) || defined(__unix__)
#include <unistd.h>
#endif

/* ThreadArg will be defined later, but is needed immediately */
struct ThreadArg__struct;

/* and choose our threads */
#ifdef _POSIX_THREADS
#define GGGGC_THREADS_POSIX 1
#include "threads-posix.h"
#else
#error Unsupported threading platform.
#endif

#endif
