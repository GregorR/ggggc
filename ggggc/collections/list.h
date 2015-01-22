/*
 * List collections for GGGGC
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

#ifndef GGGGC_COLLECTIONS_LIST_H
#define GGGGC_COLLECTIONS_LIST_H 1

#include "../gc.h"

/* generic list type */
GGC_TYPE(GGC_ListNode)
    GGC_MPTR(GGC_ListNode, next);
    GGC_MPTR(void *, el);
GGC_END_TYPE(GGC_ListNode,
    GGC_PTR(GGC_ListNode, next)
    GGC_PTR(GGC_ListNode, el)
    )

GGC_TYPE(GGC_List)
    GGC_MDATA(ggc_size_t, length);
    GGC_MPTR(GGC_ListNode, head);
    GGC_MPTR(GGC_ListNode, tail);
GGC_END_TYPE(GGC_List,
    GGC_PTR(GGC_List, head)
    GGC_PTR(GGC_List, tail)
    )

/* push an element to the end of a generic list */
void GGC_ListPush(GGC_List list, void *value);

/* convert a list to an array */
GGC_voidpArray GGC_ListToArray(GGC_List list);

/* declarations for typed lists and their functions */
#define GGC_LIST(type) \
GGC_TYPE(type ## ListNode) \
    GGC_MPTR(type ## ListNode, next); \
    GGC_MPTR(type, el); \
GGC_END_TYPE(type ## ListNode, \
    GGC_PTR(type ## ListNode, next) \
    GGC_PTR(type ## ListNode, el) \
    ); \
\
GGC_TYPE(type ## List) \
    GGC_MDATA(ggc_size_t, length); \
    GGC_MPTR(type ## ListNode, head); \
    GGC_MPTR(type ## ListNode, tail); \
GGC_END_TYPE(type ## List, \
    GGC_PTR(type ## List, head) \
    GGC_PTR(type ## List, tail) \
    ); \
\
static void type ## ListPush(type ## List list, type value) \
{ \
    GGC_ListPush((GGC_List) list, value); \
} \
static type ## Array type ## ListToArray(type ## List list) \
{ \
    return (type ## Array) GGC_ListToArray((GGC_List) list); \
}

#endif
