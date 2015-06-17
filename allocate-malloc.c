/*
 * Allocation functions (malloc)
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

static void *allocPool(int mustSucceed)
{
    static ggc_mutex_t poolLock = GGC_MUTEX_INITIALIZER;
    static unsigned char *space = NULL, *spaceEnd = NULL;
    void *ret;

    /* do we already have some available space? */
    ggc_mutex_lock_raw(&poolLock);
    if (!space || space + GGGGC_POOL_BYTES > spaceEnd) {
        ggc_size_t i;

        /* since we can't pre-align, align by getting as much as we can manage */
        for (i = 16; i >= 2; i /= 2) {
            space = malloc(GGGGC_POOL_BYTES * i);
            if (space) break;
        }
        if (!space) {
            if (mustSucceed) {
                perror("malloc");
                abort();
            }
            return NULL;
        }
        spaceEnd = space + GGGGC_POOL_BYTES * i;

        /* align it */
        space = (unsigned char *) GGGGC_POOL_OF(space + GGGGC_POOL_BYTES - 1);
    }

    ret = (struct GGGGC_Pool *) space;
    space += GGGGC_POOL_BYTES;
    ggc_mutex_unlock(&poolLock);

    return ret;
}
