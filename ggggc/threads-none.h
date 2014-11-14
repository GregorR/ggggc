/*
 * Stubs when there are no threads
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

#ifndef GGGGC_THREADS_NONE_H
#define GGGGC_THREADS_NONE_H 1

/* functions */
#define ggc_barrier_destroy(x)          0
#define ggc_barrier_init(x,y,z)         0
#define ggc_barrier_wait(x)             0
#define ggc_barrier_wait_raw(x)         0
#define ggc_mutex_lock(x)               0
#define ggc_mutex_lock_raw(x)           0
#define ggc_mutex_trylock(x)            0
#define ggc_mutex_unlock(x)             0
#define ggc_thread_create(x,y,z)        (-1)
#define ggc_thread_join(x)              (-1)

/* types */
#define ggc_barrier_t   int
#define ggc_mutex_t     int
#define ggc_thread_t    int

/* predefs */
#define GGC_MUTEX_INITIALIZER 0

#endif
