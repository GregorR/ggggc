/*
 * Part of the ggggcify tool, which helps make writing GGGGC-using code easier.
 * This should be included implicitly via ggggc/gc.h.
 *
 * Copyright (c) 2015 Gregor Richards
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

#ifndef GGGGC_GGGGCIFY_H
#define GGGGC_GGGGCIFY_H 1

/* In ggggcify, simply use GGC() to define GGC structs */
#define GGC(x) \
    typedef struct x ## __ggggc_struct *x; \
    typedef struct x ## __ggggc_parray *x ## Array; \
    struct x ## __ggggc_parray { size_t length; x a[1]; x a__ptrs[1]; }; \
    struct x ## __ggggc_struct

/* all other means of defining/accessing are stubbed */
#undef GGC_DA_TYPE
#define GGC_DA_TYPE(type) \
    typedef struct type ## __ggggc_darray *GGC_ ## type ## _Array; \
    struct type ## __ggggc_darray { \
        size_t length; \
        type a[1]; \
        type a__data[1]; \
    };
#undef GGC_PA_TYPE
#define GGC_PA_TYPE(type) \
    typedef struct type ## __ggggc_parray *type ## Array; \
    struct type ## __ggggc_parray { \
        size_t length; \
        type a[1]; \
        type a__ptrs[1]; \
    };
#undef GGC_NEW
#define GGC_NEW(type) ((type) NULL)
#undef GGC_TYPE
#define GGC_TYPE(type) GGC(type) {
#undef GGC_END_TYPE
#define GGC_END_TYPE(type, ptrs) };
#undef GGC_WP
#undef GGC_WD
#define GGC_WP(o, m, v) ((o)->m = (v))
#define GGC_WD(o, m, v) ((o)->m = (v))
#undef GGC_RP
#undef GGC_RD
#define GGC_RP(o, m) ((o)->m)
#define GGC_RD(o, m) ((o)->m)
#undef GGC_WAP
#undef GGC_WAD
#define GGC_WAP(o, i, v) ((o)->a[i] = (v))
#define GGC_WAD(o, i, v) ((o)->a[i] = (v))
#undef GGC_RAP
#undef GGC_RAD
#define GGC_RAP(o, i) ((o)->a[i])
#define GGC_RAD(o, i) ((o)->a[i])
#undef GGC_WUP
#define GGC_WUP(o, v) ((void **) *(o) = (v))
#undef GGC_RUP
#define GGC_RUP(o) ((void **) *(o))
#undef GGC_LENGTH
#define GGC_LENGTH(o) (1)

#endif
