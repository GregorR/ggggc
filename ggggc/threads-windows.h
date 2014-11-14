/*
 * Thread support for Windows
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

#ifndef GGGGC_THREADS_WINDOWS_H
#define GGGGC_THREADS_WINDOWS_H 1

/* functions */
#define ggc_barrier_destroy(barrier)    (!DeleteSynchronizationBarrier(barrier))
#define ggc_barrier_init(barrier, count) \
    (!InitializeSynchronizationBarrier(barrier, count, -1))
#define ggc_barrier_wait_raw(barrier)   (!EnterSynchronizationBarrier(barrier, 0))
#define ggc_mutex_lock_raw(mutex)       (WaitForSingleObject(*(mutex), INFINITE) != WAIT_FAILED)
#define ggc_mutex_trylock(mutex)        (WaitForSingleObject(*(mutex), 0) != WAIT_FAILED)
#define ggc_mutex_unlock(mutex)         (!ReleaseMutex(*(mutex)))

/* types */
#define ggc_barrier_t   SYNCHRONIZATION_BARRIER
#define ggc_mutex_t     HANDLE
#define ggc_thread_t    HANDLE

/* predefs */
#define GGC_MUTEX_INITIALIZER 0

/* real code below */

/* functions */
int ggc_barrier_wait(ggc_barrier_t *barrier);
int ggc_mutex_lock(ggc_mutex_t *mutex);
int ggc_thread_create(ggc_thread_t *thread, void (*func)(struct ThreadArg__struct *), struct ThreadArg__struct *arg);
int ggc_thread_join(ggc_thread_t thread);

#endif
