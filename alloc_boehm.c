/*
 * Boehm-utilizing allocator and globals.
 *
 * Copyright (c) 2013 Gregor Richards
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

#include <string.h>

#include "ggggc.h"

size_t GGGGC_fytheConstBankPtrs = 0;
void *GGGGC_fytheConstBank = NULL;
void *GGGGC_fytheStackBase = NULL;
void *GGGGC_fytheStackTop = NULL;

void *GGGGC_malloc(size_t sz, unsigned short ptrs, int init)
{
    void **pt;
    struct GGGGC_Header *ret = (struct GGGGC_Header *) GC_malloc(sz);

    /* configure it if this is a direct mutator allocation */
    ret->sz = sz;
    ret->gen = 0;
    ret->ptrs = ptrs;

    if (init) {
        pt = (void *) (ret + 1);
        while (ptrs--) *pt++ = NULL;
    }

    return (void *) (ret + 1);
}

void *GGGGC_malloc_ptr_array(size_t sz, int init)
{
    return GGGGC_malloc(sizeof(struct GGGGC_Header) + sz * sizeof(void *), sz, init);
}

void *GGGGC_realloc_ptr_array(void *orig, size_t sz)
{
    struct GGGGC_Header *header = (struct GGGGC_Header *) orig - 1;
    void *ret;

    /* just allocate and copy out the original */
    ret = GGGGC_malloc_ptr_array(sz, 1);
    sz = sz * sizeof(void *);
    if (header->sz - sizeof(struct GGGGC_Header) < sz) {
        memcpy(ret, orig, header->sz - sizeof(struct GGGGC_Header));
    } else {
        memcpy(ret, orig, sz);
    }

    return ret;
}

void *GGGGC_malloc_data_array(size_t sz)
{
    void **pt;
    struct GGGGC_Header *ret = (struct GGGGC_Header *) GC_malloc(sz);

    /* configure it if this is a direct mutator allocation */
    ret->sz = sz;
    ret->gen = 0;
    ret->ptrs = 0;

    return (void *) (ret + 1);
}
