/*
 * GGGGC: Gregor's General Generational Garbage Collector
 * Macros to disable GGGGC and simply use libgc
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

/* intentionally ambiguous with ggggc/gc.h */
#ifndef GGGGC_GC_H
#define GGGGC_GC_H 1

#define GGGGC_NOGGGGC 1

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "gc/gc.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ggc_size_t
#define ggc_size_t size_t
#endif

#define GGGGC_EMPTY

/* GC header (this shape must be shared by all GC'd objects) */
struct GGGGC_Header {
    struct GGGGC_Descriptor *descriptor;
};

/* GGGGC descriptors */
struct GGGGC_Descriptor {
    void *user__ptr; /* for the user to use however they please */
};

/* macros for defining types 
 * Example:
 * GGC_TYPE(Foo)
 *     GGC_MPTR(Bar, fooMemberOfTypeBar);
 *     GGC_MDATA(int, fooMemberOfTypeInt);
 * GGC_END_TYPE(Foo,
 *     GGC_PTR(Foo, fooMemberOfTypeBar)
 *     );
 */
#define GGC_DA_TYPE(type) \
    static struct GGGGC_Descriptor GGC_ ## type ## _Array__descriptor; \
    typedef struct type ## __gc_array *GGC_ ## type ## _Array; \
    struct type ## __gc_array { \
        struct GGGGC_Header header; \
        size_t length; \
        type a__data[1]; \
    }; \
    static GGC_ ## type ## _Array GGC_ ## type ## _ArrayNew(size_t sz) { \
        GGC_ ## type ## _Array ret = GC_MALLOC(sizeof(struct type ## __gc_array) + sz * sizeof(type)); \
        ret->header.descriptor = &GGC_ ## type ## _Array__descriptor; \
        return ret; \
    }
#define GGC_PA_TYPE(type) \
    static struct GGGGC_Descriptor type ## Array__descriptor; \
    typedef struct type ## __gc_array *type ## Array; \
    struct type ## __gc_array { \
        struct GGGGC_Header header; \
        size_t length; \
        type a__ptrs[1]; \
    }; \
    static type ## Array type ## ArrayNew(size_t sz) { \
        type ## Array ret = GC_MALLOC(sizeof(struct type ## __gc_array) + sz * sizeof(void*)); \
        ret->header.descriptor = &type ## Array__descriptor; \
        return ret; \
    }
#define GGC_TYPE(type) \
    static struct GGGGC_Descriptor type ## __descriptor; \
    typedef struct type ## __struct *type; \
    GGC_PA_TYPE(type) \
    struct type ## __struct { \
        struct GGGGC_Header header;
#define GGC_MDATA(type, name) \
        type name
#define GGC_MPTR(type, name) \
        type name
#define GGC_END_TYPE(type, pointers) \
    }; \
    static type type ## New() { \
        type ret = GC_MALLOC(sizeof(struct type ## __struct)); \
        ret->header.descriptor = &type ## __descriptor; \
        return ret; \
    }

/* a few simple builtin types */
GGC_DA_TYPE(char)
GGC_DA_TYPE(short)
GGC_DA_TYPE(int)
GGC_DA_TYPE(unsigned)
GGC_DA_TYPE(long)
GGC_DA_TYPE(float)
GGC_DA_TYPE(double)
GGC_DA_TYPE(size_t)

/* write barriers */
#define GGC_WP(object, member, value) object->member = value
#define GGC_WD(object, member, value) object->member = value
#define GGC_WAP(object, index, value) object->a__ptrs[index] = value
#define GGC_WAD(object, index, value) object->a__data[index] = value
#define GGC_WUP(object, value) object->header.descriptor->user__ptr

/* read barriers */
#define GGC_RP(object, member) (object)->member
#define GGC_RD(object, member) (object)->member
#define GGC_RAP(object, index) (object)->a__ptrs[index]
#define GGC_RAD(object, index) (object)->a__data[index]
#define GGC_RUP(object)        (object)->header.descriptor->user__ptr

#define GGC_NULL NULL

/* general allocator */
#define GGC_NEW(type) type ## New()

/* allocate a pointer array (size is in words) */
#define GGC_NEW_PA(type, size) type ## ArrayNew((size))

/* allocate a data array (size is in words, macro turns it into elements) */
#define GGC_NEW_DA(type, size) ((GGC_ ## type ## _Array) GC_MALLOC(sizeof(struct type ## __gc_array) + (size) * sizeof(type)))

/* stuff we don't need */
#define GGC_YIELD() 0
#define GGC_GLOBALIZE() 0
#define GGC_POP() 0
#define GGGGC_DEBUG_NOPUSH
#include "push.h"
#undef GGGGC_DEBUG_NOPUSH

#ifdef __cplusplus
}
#endif


#endif
