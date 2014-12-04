/*
 * List collections for GGGGC
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

#ifndef GGGGC_COLLECTIONS_LIST_H
#define GGGGC_COLLECTIONS_LIST_H 1

#include "../gc.h"

/* declarations for type ## ListNode (the list node type) and type ## List (the
 * list type), as well as type ## ListPush and type ## ListToArray (list
 * functions) */
#define GGGGC_LIST_DECL(type, static) \
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
static void type ## ListPush(type ## List list, type value); \
static type ## Array type ## ListToArray(type ## List list)

#define GGC_LIST_DECL_STATIC(type) GGGGC_LIST_DECL(type, static)
#define GGC_LIST_DECL(type) GGGGC_LIST_DECL(type, GGGGC_EMPTY)

/* the definitions of our list functions */
#define GGGGC_LIST_DEFN(type, static) \
static void type ## ListPush(type ## List list, type value) \
{ \
    type ## ListNode node = NULL, tail = NULL; \
    \
    GGC_PUSH_4(list, value, node, tail); \
    \
    node = GGC_NEW(type ## ListNode); \
    GGC_W(node, el, value); \
    \
    tail = GGC_R(list, tail); \
    if (tail) \
        GGC_W(tail, next, node); \
    else \
        GGC_W(list, head, node); \
    GGC_W(list, tail, node); \
    \
    list->length++; \
    \
    return; \
} \
\
static type ## Array type ## ListToArray(type ## List list) \
{ \
    type ## Array ret = NULL; \
    type ## ListNode curn = NULL; \
    type cur = NULL; \
    size_t i = 0; \
    \
    GGC_PUSH_4(list, ret, curn, cur); \
    ret = GGC_NEW_PA(type, list->length); \
    \
    for (i = 0, curn = GGC_R(list, head); \
         i < list->length && curn; \
         i++, curn = GGC_R(curn, next)) { \
         cur = GGC_R(curn, el); \
         GGC_WA(ret, i, cur); \
    } \
    \
    return ret; \
}

#define GGC_LIST_DEFN_STATIC(type) GGGGC_LIST_DEFN(type, static)
#define GGC_LIST_DEFN(type) GGGGC_LIST_DEFN(type, GGGGC_EMPTY)

/* and both together */
#define GGGGC_LIST(type, static) \
    GGGGC_LIST_DECL(type, static); \
    GGGGC_LIST_DEFN(type, static)
#define GGC_LIST_STATIC(type) GGGGC_LIST(type, static)
/* no GGC_LIST without _STATIC, as that makes no sense */

#endif
