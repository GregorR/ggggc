#ifndef GGGGC_THREAD_LOCALS_H
#define GGGGC_THREAD_LOCALS_H 1

#if defined(GGGGC_NO_THREADS)
/* no threads = no thread locals either */
#define ggc_thread_local

#elif __STDC_VERSION__ >= 201112L
#define ggc_thread_local _Thread_local

#elif defined(__GNUC__)
#define ggc_thread_local __thread

#else
#warning No known thread-local storage specifier. Disabling threads.
#define ggc_thread_local
#define GGGGC_NO_THREADS 1

#endif

#endif
