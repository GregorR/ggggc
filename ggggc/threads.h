#ifndef GGGGC_THREADS_H 
#define GGGGC_THREADS_H 1

#ifndef GGGGC_NO_THREADS

/* get our feature macros */
#if defined(unix) || defined(__unix) || defined(__unix__)
#include <unistd.h>
#endif

/* choose our threads */
#ifdef _POSIX_THREADS
#define GGGGC_THREADS_POSIX
#else
#define GGGGC_NO_THREADS
#warning Unsupported threading platform. Disabling threads.
#endif

#endif
#ifndef GGGGC_NO_THREADS

/* blocking on thread behaviors is unsafe unless the GC is informed */
void ggc_pre_blocking(void);
void ggc_post_blocking(void);

/* ThreadArg will be defined later, but is needed immediately */
struct ThreadArg__struct;

/* and choose our threads */
#if defined(GGGGC_THREADS_POSIX)
#include "threads-posix.h"
#endif

#else
#include "threads-none.h"

#endif

#endif
