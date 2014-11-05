#ifndef GGGGC_THREAD_LOCALS_H
#define GGGGC_THREAD_LOCALS_H 1

#if __STDC_VERSION__ >= 201112L
#define ggc_thread_local _Thread_local

#elif defined(__GNUC__)
#define ggc_thread_local __thread

#else
#error No known thread-local storage specifier.

#endif

#endif
