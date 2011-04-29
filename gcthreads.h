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

#ifndef GGGGC_GCTHREADS_H
#define GGGGC_GCTHREADS_H

#ifndef GGGGC_GCTHREADS_HFROMC
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

/* equivalent to pthread_create */
int GGC_thread_create(GGC_thread_t thread, void *(*start_routine)(void *), void *arg);

/* equivalent to pthread_mutex_init */
int GGC_mutex_init(GGC_th_mutex_t mutex);

/* equivalent to pthread_mutex_destroy */
int GGC_mutex_destroy(GGC_th_mutex_t mutex);

/* equivalent to pthread_mutex_lock */
int GGC_mutex_lock(GGC_th_mutex_t mutex);

/* equivalent to pthread_mutex_unlock */
int GGC_mutex_unlock(GGC_th_mutex_t mutex);

/* equivalent to pthread_rwlock_init */
int GGC_rwlock_init(GGC_th_rwlock_t rwlock);

/* equivalent to pthread_rwlock_destroy */
int GGC_rwlock_destroy(GGC_th_rwlock_t rwlock);

/* equivalent to pthread_rwlock_rdlock */
int GGC_rwlock_rdlock(GGC_th_rwlock_t rwlock);

/* equivalent to pthread_rwlock_unlock (for readers) */
int GGC_rwlock_rdunlock(GGC_th_rwlock_t rwlock);

/* equivalent to pthread_rwlock_wrlock */
int GGC_rwlock_wrlock(GGC_th_rwlock_t rwlock);

/* equivalent to pthread_rwlock_unlock (for writers) */
int GGC_rwlock_wrunlock(GGC_th_rwlock_t rwlock);

/* equivalent to pthread_barrier_init */
int GGC_barrier_init(GGC_th_barrier_t barrier, unsigned count);

/* equivalent to pthread_barrier_destroy */
int GGC_barrier_destroy(GGC_th_barrier_t barrier);

/* equivalent to pthread_barrier_wait */
int GGC_barrier_wait(GGC_th_barrier_t barrier);
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


/* figure out what the best kind of TLS to use is */
#if defined(__MACH__)
/* for now, no true TLS annotation on Mach-O :( */
#define GGGGC_TLS_DISPATCH

#elif defined(__GNUC__)
#define GGGGC_TLS_ANN __thread

#elif defined(_WIN32)
#define GGGGC_TLS_ANN __declspec(thread)

#elif defined(__SUNPRO_CC)
#define GGGGC_TLS_ANN __thread

#else
#define GGGGC_TLS_DISPATCH

#endif


/* now macros to create TLS variables */
#if defined(GGGGC_TLS_ANN)
#define GGC_TLS(type)           GGGGC_TLS_ANN type
#define GGC_TLS_INIT(var)
#define GGC_TLS_SET(var, val)   ((var) = (val))
#define GGC_TLS_GET(type, var)  ((type) (var))

#elif defined(GGGGC_TLS_DISPATCH)
#define GGC_TLS(type)           GGC_th_key_t
#define GGC_TLS_INIT(var)       do { \
                                    (var) = GGC_alloc_key(); \
                                    GGC_key_init(var); \
                                } while (0)
#define GGC_TLS_SET(var, val)   (GGC_key_set((var), (void *) (size_t) (val)))
#define GGC_TLS_GET(type, var)  ((type) (size_t) GGC_key_get(var))

#endif


/* global thread identifier */
extern GGC_TLS(void *) GGC_thread_identifier;


#endif
