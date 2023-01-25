/*
 * Allocation functions (sbrk)
 *
 * Copyright (c) 2022 Gregor Richards
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

/* Offset of sbrk from a pool boundary */
static size_t poolOffset()
{
    void *brk = sbrk(0);
    size_t offset = GGGGC_POOL_BYTES - ((size_t) brk & GGGGC_POOL_INNER_MASK);
    if (offset == GGGGC_POOL_BYTES)
        return 0;
    else
        return offset;
}

/* Attempt to bump sbrk using malloc, so that the intervening space isn't
 * wasted */
static void mallocBump()
{
    size_t prevOffset = -1, offset = poolOffset();
    void **list = NULL;
    static int failure = 0;

    if (failure)
        return;

    while (offset > 4 * sizeof(void *) && offset < prevOffset) {
        /* try to give some space to malloc */
        void **next = malloc(offset >> 2);
        if (!next) break;
        next[0] = list;
        list = next;
        prevOffset = offset;
        offset = poolOffset();
    }

    if (offset > prevOffset)
        failure = 1;

    /* return it all */
    while (list) {
        void **next = (void **) list[0];
        free(list);
        list = next;
    }
}

static void *allocPool(int mustSucceed)
{
    void *ret;
    size_t offset = poolOffset();

    mallocBump();

    offset = poolOffset();
    if (offset != 0) {
        /* align manually */
        fprintf(stderr, "WARNING: Wasting %d bytes\n", (int) offset);
        sbrk(offset);
    }

    ret = sbrk(GGGGC_POOL_BYTES);
    if (ret == (void *) -1) {
        if (mustSucceed) {
            perror("sbrk");
            abort();
        } else {
            return NULL;
        }
    }
    return ret;
}
