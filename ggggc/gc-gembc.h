/*
 * GGGGC-gembc: Generational, en-masse promotion, break-table compaction
 * algorithm
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

#ifndef GGGGC_GC_GEMBC_H
#define GGGGC_GC_GEMBC_H 1

#define GGGGC_COLLECTOR_GEMBC 1

/* pool members */
#define GGGGC_COLLECTOR_POOL_MEMBERS_BREAK_TABLE \
    /* size of the break table (in entries, used only during collection) */ \
    ggc_size_t breakTableSize; \
    \
    /* pointer to the break table (used only during collection) */ \
    void *breakTable;

#if GGGGC_GENERATIONS > 1
#define GGGGC_COLLECTOR_POOL_MEMBERS \
    /* the remembered set for this pool. NOTE: It's important this be first to \
     * make assigning to the remembered set take one less operation */ \
    unsigned char remember[GGGGC_CARDS_PER_POOL]; \
    \
    /* the locations of objects within the cards */ \
    unsigned short firstObject[GGGGC_CARDS_PER_POOL]; \
    \
    /* the generation of this pool */ \
    unsigned char gen; \
    \
    GGGGC_COLLECTOR_POOL_MEMBERS_BREAK_TABLE

#else
#define GGGGC_COLLECTOR_POOL_MEMBERS GGGGC_COLLECTOR_POOL_MEMBERS_BREAK_TABLE

#endif

/* write barriers */
#if GGGGC_GENERATIONS > 1
#define GGGGC_WP(object, member, value) do { \
    ggc_size_t ggggc_o = (ggc_size_t) (object); \
    struct GGGGC_Pool *ggggc_pool = GGGGC_POOL_OF(ggggc_o); \
    GGGGC_ASSERT_ID(object); \
    GGGGC_ASSERT_ID(value); \
    if (ggggc_pool->gen) { \
        /* a high-gen object, let's remember it */ \
        ggggc_pool->remember[GGGGC_CARD_OF(ggggc_o)] = 1; \
    } \
    (object)->member = (value); \
} while(0)
#else
#define GGGGC_WP(object, member, value) do { \
    GGGGC_ASSERT_ID(object); \
    GGGGC_ASSERT_ID(value); \
    (object)->member = (value); \
} while(0)
#endif
#define GGGGC_WD(object, member, value) do { \
    GGGGC_ASSERT_ID(object); \
    GGGGC_ASSERT_ID(if_not_a_value_then_ ## value); \
    (object)->member = (value); \
} while(0)

/* and read barriers (or lack thereof) */
#define GGGGC_RP(object, member)  ((object)->member)
#define GGGGC_RD(object, member)  ((object)->member)

#endif
