#ifndef GGGGC_THREADS_H 
#define GGGGC_THREADS_H 1

/* get our feature macros */
#if defined(unix) || defined(__unix) || defined(__unix__)
#include <unistd.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

/* blocking on thread behaviors is unsafe unless the GC is informed */
void ggc_pre_blocking(void);
void ggc_post_blocking(void);

/* ThreadArg will be defined later, but is needed immediately */
struct ThreadArg__struct;

/* choose our threads */
#if defined(GGGGC_NO_THREADS)
#include "threads-none.h"

#elif defined(_POSIX_THREADS)
#define GGGGC_THREADS_POSIX 1
#include "threads-posix.h"

#elif defined(_WIN32)
#define GGGGC_THREADS_WINDOWS 1
#include "threads-windows.h"

#else
#define GGGGC_NO_THREADS 1
#warning Unsupported threading platform. Disabling threads.
#include "threads-none.h"

#endif

#endif
