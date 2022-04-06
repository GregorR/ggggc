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
    void *brk = sbrk(0);
    size_t offset = poolOffset();

    if (offset > 3 * sizeof(void *)) {
        /* enough space to safely try this */
        void **list = NULL;
        while (sbrk(0) == brk) {
            void **next = malloc(offset - 2 * sizeof(void *));
            if (!next) break;
            next[0] = list;
            list = next;
        }

        /* return it all */
        while (list) {
            void **next = list[0];
            free(list);
            list = next;
        }
    }
}

static void *allocPool(int mustSucceed)
{
    void *ret;
    size_t offset = poolOffset();

    if (offset != 0) {
        /* We're not aligned. Try to get closer with malloc bumping. */
        mallocBump();
        mallocBump();
    }

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
