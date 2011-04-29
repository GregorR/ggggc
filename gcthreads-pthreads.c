/*
 * GGGGC threads (pthreads)
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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "helpers.h"

struct _GGC_thread_t {
    pthread_t v;
};
typedef struct _GGC_thread_t *GGC_thread_t;

struct _GGC_th_mutex_t {
    pthread_mutex_t v;
};
typedef struct _GGC_th_mutex_t *GGC_th_mutex_t;

struct _GGC_th_barrier_t {
    pthread_barrier_t v;
};
typedef struct _GGC_th_barrier_t *GGC_th_barrier_t;

struct _GGC_th_key_t {
    pthread_key_t v;
};
typedef struct _GGC_th_key_t *GGC_th_key_t;

/* to define our own types */
#define GGGGC_GCTHREADS_HFROMC
#include "gcthreads.h"
#include "ggggc.h"
#include "ggggc_internal.h"

/* global thread ID info */
static unsigned int curThreadId;
GGC_th_mutex_t curThreadIdLock;

/* initialize GGGGC's threading subsystem */
void GGC_threads_init()
{
    GGC_threads_init_common();
    curThreadId = 1;
    curThreadIdLock = GGC_alloc_mutex();
    GGC_mutex_init(curThreadIdLock);
}

/* allocate a thread object */
GGC_thread_t GGC_alloc_thread()
{
    GGC_thread_t ret;
    SF(ret, malloc, NULL, (sizeof(struct _GGC_thread_t)));
    return ret;
}

/* free a thread object */
void GGC_free_thread(GGC_thread_t thread)
{
    free(thread);
}

/* get a mutex object (FIXME: it's less than ideal that this is allocated ...) */
GGC_th_mutex_t GGC_alloc_mutex()
{
    GGC_th_mutex_t ret;
    SF(ret, malloc, NULL, (sizeof(struct _GGC_th_mutex_t)));
    return ret;
}

/* free a mutex object */
void GGC_free_mutex(GGC_th_mutex_t mutex)
{
    free(mutex);
}

/* get a barrier object (FIXME: it's less than ideal that this is allocated ...) */
GGC_th_barrier_t GGC_alloc_barrier()
{
    GGC_th_barrier_t ret;
    SF(ret, malloc, NULL, (sizeof(struct _GGC_th_barrier_t)));
    return ret;
}

/* free a barrier object */
void GGC_free_barrier(GGC_th_barrier_t barrier)
{
    free(barrier);
}

/* get a key object (FIXME: it's less than ideal that this is allocated ...) */
GGC_th_key_t GGC_alloc_key()
{
    GGC_th_key_t ret;
    SF(ret, malloc, NULL, (sizeof(struct _GGC_th_key_t)));
    return ret;
}

/* free a key object */
void GGC_free_key(GGC_th_key_t key)
{
    free(key);
}

/* landing function for GGC_thread_create */
static void *GGGGC_thread_child(void *args)
{
    void *(*real)(void *) = ((void *(**)(void *)) args)[0];
    void *rarg = ((void **) args)[1];
    free(args);

    /* get a thread ID */
    GGC_mutex_lock(curThreadIdLock);
    GGC_key_set(GGC_thread_identifier, (void *) (size_t) curThreadId++);
    GGC_mutex_unlock(curThreadIdLock);

    /* tell GGGGC we exist */
    GGGGC_new_thread();

    /* then call the real function */
    return real(rarg);
}

/* equivalent to pthread_create */
int GGC_thread_create(GGC_thread_t thread, void *(*start_routine)(void *), void *arg)
{
    void **trampoline;

    SF(trampoline, malloc, NULL, (2 * sizeof(void*)));
    trampoline[0] = (void *) (size_t) start_routine;
    trampoline[1] = arg;

    return pthread_create(&thread->v, NULL, GGGGC_thread_child, trampoline);
}

/* equivalent to pthread_mutex_init */
int GGC_mutex_init(GGC_th_mutex_t mutex)
{
    return pthread_mutex_init(&mutex->v, NULL);
}

/* equivalent to pthread_mutex_destroy */
int GGC_mutex_destroy(GGC_th_mutex_t mutex)
{
    return pthread_mutex_destroy(&mutex->v);
}

/* equivalent to pthread_mutex_lock */
int GGC_mutex_lock(GGC_th_mutex_t mutex)
{
    return pthread_mutex_lock(&mutex->v);
}

/* equivalent to pthread_mutex_unlock */
int GGC_mutex_unlock(GGC_th_mutex_t mutex)
{
    return pthread_mutex_unlock(&mutex->v);
}

/* equivalent to pthread_barrier_init */
int GGC_barrier_init(GGC_th_barrier_t barrier, unsigned count)
{
    return pthread_barrier_init(&barrier->v, NULL, count);
}

/* equivalent to pthread_barrier_destroy */
int GGC_barrier_destroy(GGC_th_barrier_t barrier)
{
    return pthread_barrier_destroy(&barrier->v);
}

/* equivalent to pthread_barrier_wait */
int GGC_barrier_wait(GGC_th_barrier_t barrier)
{
    int ret = pthread_barrier_wait(&barrier->v);
    if (ret == PTHREAD_BARRIER_SERIAL_THREAD) ret = GGC_BARIER_SERIAL_THREAD;
    else ret = -1;
    return ret;
}

/* equivalent to pthread_key_create */
int GGC_key_init(GGC_th_key_t key)
{
    return pthread_key_create(&key->v, NULL);
}

/* equivalent to pthread_key_delete */
int GGC_key_destroy(GGC_th_key_t key)
{
    return pthread_key_delete(key->v);
}

/* equivalent to pthread_setspecific */
int GGC_key_set(GGC_th_key_t key, const void *val)
{
    return pthread_setspecific(key->v, val);
}

/* equivalent to pthread_getspecific */
void *GGC_key_get(GGC_th_key_t key)
{
    return pthread_getspecific(key->v);
}

/* portable compare-and-swap operation, falling back to mutex iff necessary */
int GGC_cas(GGC_th_mutex_t mutex, void **addr, void *oldv, void *newv)
{
#ifdef __GNUC__
    return __sync_bool_compare_and_swap(addr, oldv, newv);
#else
    int ret = 1;
    GGC_mutex_lock(mutex);
    if (*addr == oldv) {
        *addr = newv;
    } else {
        ret = 0;
    }
    GGC_mutex_unlock(mutex);
    return ret;
#endif
}
