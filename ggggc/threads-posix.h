#ifndef GGGGC_THREADS_POSIX_H
#define GGGGC_THREADS_POSIX_H 1

#include <pthread.h>

#define ggc_barrier_t   pthread_barrier_t
#define ggc_mutex_t     pthread_mutex_t
#define ggc_thread_t    pthread_t

#endif
