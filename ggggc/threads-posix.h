#ifndef GGGGC_THREADS_POSIX_H
#define GGGGC_THREADS_POSIX_H 1

#include <pthread.h>

/* functions */
#define ggc_barrier_init    pthread_barrier_init
#define ggc_barrier_wait    pthread_barrier_wait
#define ggc_mutex_lock      pthread_mutex_lock
#define ggc_mutex_trylock   pthread_mutex_trylock
#define ggc_mutex_unlock    pthread_mutex_unlock

/* types */
#define ggc_barrier_t   pthread_barrier_t
#define ggc_mutex_t     pthread_mutex_t
#define ggc_thread_t    pthread_t

/* predefs */
#define GGC_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

#endif
