#ifndef GGGGC_THREADS_H 
#define GGGGC_THREADS_H 1

/* get our feature macros */
#if defined(unix) || defined(__unix) || defined(__unix__)
#include <unistd.h>
#endif

#ifdef _POSIX_THREADS
#include "threads-posix.h"
#else
#error Unsupported threading platform.
#endif

#endif
