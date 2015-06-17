/*
 * Generic barriers using mutexes and semaphores
 *
 * Copyright (c) 2014, 2015 Gregor Richards
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

int ggc_barrier_destroy(ggc_barrier_t *barrier)
{
    return ggc_sem_destroy(&barrier->waiters);
}

int ggc_barrier_init(ggc_barrier_t *barrier, unsigned long ct)
{
    barrier->lock = GGC_MUTEX_INITIALIZER;
    barrier->cur = 0;
    barrier->ct = ct;
    ggc_sem_init(&barrier->waiters, ct - 1);
    return 0;
}

int ggc_barrier_wait_raw(ggc_barrier_t *barrier)
{
    ggc_mutex_lock_raw(&barrier->lock);

    /* if we're the last one, great */
    if (barrier->cur == barrier->ct - 1) {
        unsigned long i;

        /* signal all the others */
        for (i = 0; i < barrier->ct; i++) ggc_sem_post(&barrier->waiters);

        /* and reset the barrier */
        barrier->cur = 0;
        ggc_mutex_unlock(&barrier->lock);
        return 0;
    }

    /* otherwise, wait on the others */
    barrier->cur++;
    ggc_mutex_unlock(&barrier->lock);
    ggc_sem_wait_raw(&barrier->waiters);

    return 0;
}

int ggc_barrier_wait(ggc_barrier_t *barrier)
{
    int ret;
    ggc_pre_blocking();
    ret = ggc_barrier_wait_raw(barrier);
    ggc_post_blocking();
    return ret;
}
