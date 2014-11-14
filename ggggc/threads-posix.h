/*
 * Thread support for pthreads
 *
 * Copyright (c) 2014 Gregor Richards
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef GGGGC_THREADS_POSIX_H
#define GGGGC_THREADS_POSIX_H 1

#include <pthread.h>

/* functions */
#define ggc_barrier_destroy             pthread_barrier_destroy
#define ggc_barrier_init(x,y)           pthread_barrier_init(x,NULL,y)
#define ggc_barrier_wait_raw            pthread_barrier_wait
#define ggc_mutex_lock_raw              pthread_mutex_lock
#define ggc_mutex_trylock               pthread_mutex_trylock
#define ggc_mutex_unlock                pthread_mutex_unlock

/* types */
#define ggc_barrier_t   pthread_barrier_t
#define ggc_mutex_t     pthread_mutex_t
#define ggc_thread_t    pthread_t

/* predefs */
#define GGC_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

/* real code below */

/* functions */
int ggc_barrier_wait(void *barrier); /* void because some configurations don't have pthread_barrier_t */
int ggc_mutex_lock(ggc_mutex_t *mutex);
int ggc_thread_create(ggc_thread_t *thread, void (*func)(struct ThreadArg__struct *), struct ThreadArg__struct *arg);
int ggc_thread_join(ggc_thread_t thread);

#endif
