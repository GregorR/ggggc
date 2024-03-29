/*
 * Allocation functions (mmap)
 *
 * Copyright (c) 2014-2022 Gregor Richards
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
    unsigned char *space, *aspace;
    struct GGGGC_Pool *ret;

    /* allocate enough space that we can align it later */
    space = (unsigned char *) mmap(NULL, GGGGC_POOL_BYTES*2, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
    if (space == NULL) {
        if (mustSucceed) {
            perror("mmap");
            abort();
        }
        return NULL;
    }

    /* align it */
    ret = GGGGC_POOL_OF(space + GGGGC_POOL_BYTES - 1);
    aspace = (unsigned char *) ret;

    /* free unused space */
    if (aspace > space)
        munmap(space, aspace - space);
    munmap(aspace + GGGGC_POOL_BYTES, space + GGGGC_POOL_BYTES - aspace);

    return ret;
}
