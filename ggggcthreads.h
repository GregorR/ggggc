/*
 * GGGGC threads
 *
 * Copyright (C) 2011 Gregor Richards
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef GGGGC_THREADS_H
#define GGGGC_THREADS_H

#ifndef GGGGC_THREADS_HFROMC
typedef void *GGC_thread_t;
typedef void *GGC_th_mutex_t;
typedef void *GGC_th_rwlock_t;
typedef void *GGC_th_barrier_t;
typedef void *GGC_th_key_t;
#endif

/* initialize GGGGC's threading subsystem (usually done by GGC_init) */
void GGC_threads_init();

/* allocate a thread object */
GGC_thread_t GGC_alloc_thread();

/* free a thread object */
void GGC_free_thread(GGC_thread_t thread);

/* get a mutex object (FIXME: it's less than ideal that this is allocated ...) */
GGC_th_mutex_t GGC_alloc_mutex();

/* free a mutex object */
void GGC_free_mutex(GGC_th_mutex_t mutex);

/* get a rwlock object (FIXME: it's less than ideal that this is allocated ...) */
GGC_th_rwlock_t GGC_alloc_rwlock();

/* free a rwlock object */
void GGC_free_rwlock(GGC_th_rwlock_t rwlock);

/* get a barrier object (FIXME: it's less than ideal that this is allocated ...) */
GGC_th_barrier_t GGC_alloc_barrier();

/* free a barrier object */
void GGC_free_barrier(GGC_th_barrier_t barrier);

/* get a key object (FIXME: it's less than ideal that this is allocated ...) */
GGC_th_key_t GGC_alloc_key();

/* free a key object */
void GGC_free_key(GGC_th_key_t key);

/* equivalent to sysconf(_SC_NPROCESSORS_ONLN) */
int GGC_nprocs();

/* equivalent to pthread_create */
int GGC_thread_create(GGC_thread_t thread, void *(*start_routine)(void *), void *arg);

/* equivalent to pthread_mutex_init */
int GGGGC_mutex_init(GGC_th_mutex_t mutex);

/* equivalent to pthread_mutex_destroy */
int GGGGC_mutex_destroy(GGC_th_mutex_t mutex);

/* equivalent to pthread_mutex_lock */
int GGGGC_mutex_lock(GGC_th_mutex_t mutex);

/* equivalent to pthread_mutex_trylock (note: returns 0 on success) */
int GGGGC_mutex_trylock(GGC_th_mutex_t mutex);

/* equivalent to pthread_mutex_unlock */
int GGGGC_mutex_unlock(GGC_th_mutex_t mutex);

/* equivalent to pthread_rwlock_init */
int GGGGC_rwlock_init(GGC_th_rwlock_t rwlock);

/* equivalent to pthread_rwlock_destroy */
int GGGGC_rwlock_destroy(GGC_th_rwlock_t rwlock);

/* equivalent to pthread_rwlock_rdlock */
int GGGGC_rwlock_rdlock(GGC_th_rwlock_t rwlock);

/* equivalent to pthread_rwlock_tryrdlock */
int GGGGC_rwlock_tryrdlock(GGC_th_rwlock_t rwlock);

/* equivalent to pthread_rwlock_unlock (for readers) */
int GGGGC_rwlock_rdunlock(GGC_th_rwlock_t rwlock);

/* equivalent to pthread_rwlock_wrlock */
int GGGGC_rwlock_wrlock(GGC_th_rwlock_t rwlock);

/* equivalent to pthread_rwlock_trywrlock */
int GGGGC_rwlock_trywrlock(GGC_th_rwlock_t rwlock);

/* equivalent to pthread_rwlock_unlock (for writers) */
int GGGGC_rwlock_wrunlock(GGC_th_rwlock_t rwlock);

/* equivalent to pthread_barrier_init */
int GGGGC_barrier_init(GGC_th_barrier_t barrier, unsigned count);

/* equivalent to pthread_barrier_destroy */
int GGGGC_barrier_destroy(GGC_th_barrier_t barrier);

/* equivalent to pthread_barrier_wait */
int GGGGC_barrier_wait(GGC_th_barrier_t barrier);
#define GGC_BARRIER_SERIAL_THREAD 1

/* equivalent to pthread_key_create */
int GGC_key_init(GGC_th_key_t key);

/* equivalent to pthread_key_delete */
int GGC_key_destroy(GGC_th_key_t key);

/* equivalent to pthread_setspecific */
int GGC_key_set(GGC_th_key_t key, const void *val);

/* equivalent to pthread_getspecific */
void *GGC_key_get(GGC_th_key_t key);

/* portable compare-and-swap operation, falling back to mutex iff necessary */
int GGC_cas(GGC_th_mutex_t mutex, void **addr, void *oldv, void *newv);

#include "threads-tls.h"
#include "threads-barrier.h"

/* global thread identifier */
extern GGC_TLS(void *) GGC_thread_identifier;

/* and everything with memory barriers as appropriate */
#define GGC_THREAD_CREATE(r, x, y, z)           do { GGC_FULL_BARRIER; (r) = GGGGC_thread_create(x, y, z); (void) r; } while (0)
#define GGC_MUTEX_INIT(r, x)                    do { (r) = GGGGC_mutex_init(x); (void) r; } while (0)
#define GGC_MUTEX_DESTROY(r, x)                 do { (r) = GGGGC_mutex_destroy(x); (void) r; } while (0)
#define GGC_MUTEX_LOCK(r, x)                    do { GGC_FULL_BARRIER; (r) = GGGGC_mutex_lock(x); (void) r; GGC_FULL_BARRIER; } while (0)
#define GGC_MUTEX_TRYLOCK(r, x)                 do { GGC_FULL_BARRIER; (r) = GGGGC_mutex_trylock(x); (void) r; GGC_FULL_BARRIER; } while (0)
#define GGC_MUTEX_UNLOCK(r, x)                  do { GGC_FULL_BARRIER; (r) = GGGGC_mutex_unlock(x); (void) r; GGC_FULL_BARRIER; } while (0)
#define GGC_RWLOCK_INIT(r, x)                   do { (r) = GGGGC_rwlock_init(x); (void) r; } while (0)
#define GGC_RWLOCK_DESTROY(r, x)                do { (r) = GGGGC_rwlock_destroy(x); (void) r; } while (0)
#define GGC_RWLOCK_RDLOCK(r, x)                 do { GGC_FULL_BARRIER; (r) = GGGGC_rwlock_rdlock(x); (void) r; GGC_FULL_BARRIER; } while (0)
#define GGC_RWLOCK_TRYRDLOCK(r, x)              do { GGC_FULL_BARRIER; (r) = GGGGC_rwlock_tryrdlock(x); (void) r; GGC_FULL_BARRIER; } while (0)
#define GGC_RWLOCK_RDUNLOCK(r, x)               do { GGC_FULL_BARRIER; (r) = GGGGC_rwlock_rdunlock(x); (void) r; GGC_FULL_BARRIER; } while (0)
#define GGC_RWLOCK_WRLOCK(r, x)                 do { GGC_FULL_BARRIER; (r) = GGGGC_rwlock_wrlock(x); (void) r; GGC_FULL_BARRIER; } while (0)
#define GGC_RWLOCK_TRYWRLOCK(r, x)              do { GGC_FULL_BARRIER; (r) = GGGGC_rwlock_trywrlock(x); (void) r; GGC_FULL_BARRIER; } while (0)
#define GGC_RWLOCK_WRUNLOCK(r, x)               do { GGC_FULL_BARRIER; (r) = GGGGC_rwlock_wrunlock(x); (void) r; GGC_FULL_BARRIER; } while (0)
#define GGC_BARRIER_INIT(r, x, y)               do { (r) = GGGGC_barrier_init(x, y); (void) r; } while (0)
#define GGC_BARRIER_DESTROY(r, x)               do { (r) = GGGGC_barrier_destroy(x); (void) r; } while (0)
#define GGC_BARRIER_WAIT(r, x)                  do { GGC_FULL_BARRIER; (r) = GGGGC_barrier_wait(x); (void) r; GGC_FULL_BARRIER; } while (0)

#endif
