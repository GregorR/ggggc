/*
 * Generic barriers using mutexes and semaphores
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

#ifndef GGGGC_GEN_BARRIERS_H
#define GGGGC_GEN_BARRIERS_H 1

typedef struct ggggc_barrier_t_ {
    ggc_mutex_t lock;
    unsigned long cur, ct;
    ggc_sem_t waiters;
} ggc_barrier_t;

int ggc_barrier_destroy(ggc_barrier_t *barrier);
int ggc_barrier_init(ggc_barrier_t *barrier, unsigned long ct);
int ggc_barrier_wait(ggc_barrier_t *barrier);
int ggc_barrier_wait_raw(ggc_barrier_t *barrier);

#endif
