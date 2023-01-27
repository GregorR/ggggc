/*
 * GGGGC-portablems: Highly portable simple mark and sweep
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

#ifndef GGGGC_GC_PORTABLEMS_H
#define GGGGC_GC_PORTABLEMS_H 1

#define GGGGC_COLLECTOR_PORTABLEMS 1

#define GGGGC_USE_PORTABLE_ALLOCATOR 1

/* our freelist entries are three words, to make them easily distinguishable
 * from objects (the first word is zero) */
#undef GGGGC_MINIMUM_OBJECT_SIZE
#define GGGGC_MINIMUM_OBJECT_SIZE 3

/* portablems does not have generations */
#undef GGGGC_GENERATIONS
#define GGGGC_GENERATIONS 1

/* we can have a much smaller pool size */
#include <limits.h>
#if UINT_MAX <= 65535U && GGGGC_POOL_SIZE > 12
#undef GGGGC_POOL_SIZE
#define GGGGC_POOL_SIZE 12
#endif

/* no barriers */
#define GGGGC_WP(object, member, value) do { \
    GGGGC_ASSERT_ID(object); \
    GGGGC_ASSERT_ID(value); \
    (object)->member = (value); \
} while(0)
#define GGGGC_WD(object, member, value) do { \
    GGGGC_ASSERT_ID(object); \
    GGGGC_ASSERT_ID(if_not_a_value_then_ ## value); \
    (object)->member = (value); \
} while(0)

#define GGGGC_RP(object, member)  ((object)->member)
#define GGGGC_RD(object, member)  ((object)->member)

#endif
