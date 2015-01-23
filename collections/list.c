/*
 * Generic implementation of list collections for GGGGC
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

#include "ggggc/gc.h"
#include "ggggc/collections/list.h"

/* push an element to the end of a generic list */
void GGC_ListPush(GGC_List list, void *value)
{
    GGC_ListNode node = NULL, tail = NULL;
    ggc_size_t len;

    GGC_PUSH_4(list, value, node, tail);

    node = GGC_NEW(GGC_ListNode);
    GGC_WP(node, el, value);

    tail = GGC_RP(list, tail);
    if (tail)
        GGC_WP(tail, next, node);
    else
        GGC_WP(list, head, node);
    GGC_WP(list, tail, node);

    len = GGC_RD(list, length) + 1;
    GGC_WD(list, length, len);

    return;
}

/* push a list to the end of another list */
void GGC_ListPushList(GGC_List to, GGC_List from)
{
    GGC_ListNode head = NULL, tail = NULL;
    ggc_size_t tolen, fromlen;

    GGC_PUSH_4(to, from, head, tail);

    if (!GGC_RP(to, head)) {
        /* the target list is empty, so just graft the entire source list */
        if (!GGC_RP(from, head)) {
            /* they're both empty! */
            return;
        }
        fromlen = GGC_RD(from, length);
        head = GGC_RP(from, head);
        tail = GGC_RP(from, tail);
        GGC_WD(to, length, fromlen);
        GGC_WP(to, head, head);
        GGC_WP(to, tail, tail);
        GGC_WD(from, length, 0);
        head = NULL;
        GGC_WP(from, head, head);
        GGC_WP(from, tail, head);
        return;
    }

    /* they both exist, so we actually need to append */
    tolen = GGC_RD(to, length);
    fromlen = GGC_RD(from, length);
    tail = GGC_RP(to, tail);
    head = GGC_RP(from, head);

    /* append the nodes */
    GGC_WP(tail, next, head);

    /* reset the tail */
    tail = GGC_RP(from, tail);
    GGC_WP(to, tail, tail);

    /* add the length */
    tolen += fromlen;
    GGC_WD(to, length, tolen);

    /* and reset the old list */
    GGC_WD(from, length, 0);
    head = NULL;
    GGC_WP(from, head, head);
    GGC_WP(from, tail, head);

    return;
}

/* push an element to the beginning of a generic list */
void GGC_ListUnshift(GGC_List list, void *value)
{
    GGC_ListNode head = NULL, node = NULL;
    ggc_size_t len;

    GGC_PUSH_4(list, value, head, node);

    head = GGC_RP(list, head);
    node = GGC_NEW(GGC_ListNode);
    GGC_WP(node, next, head);
    GGC_WP(node, el, value);

    GGC_WP(list, head, node);
    if (!head)
        GGC_WP(list, tail, node);

    len = GGC_RD(list, length) + 1;
    GGC_WD(list, length, len);

    return;
}

/* push a list to the beginning of another list */
void GGC_ListUnshiftList(GGC_List to, GGC_List from)
{
    GGC_ListNode head = NULL, tail = NULL;
    ggc_size_t tolen, fromlen;

    GGC_PUSH_4(to, from, head, tail);

    if (!GGC_RP(to, head)) {
        /* the target list is empty, so just graft the entire source list */
        if (!GGC_RP(from, head)) {
            /* they're both empty! */
            return;
        }
        fromlen = GGC_RD(from, length);
        head = GGC_RP(from, head);
        tail = GGC_RP(from, tail);
        GGC_WD(to, length, fromlen);
        GGC_WP(to, head, head);
        GGC_WP(to, tail, tail);
        GGC_WD(from, length, 0);
        head = NULL;
        GGC_WP(from, head, head);
        GGC_WP(from, tail, head);
        return;
    }

    /* they both exist, so we actually need to prepend */
    tolen = GGC_RD(to, length);
    fromlen = GGC_RD(from, length);
    head = GGC_RP(to, head);
    tail = GGC_RP(from, tail);

    /* append the nodes */
    GGC_WP(tail, next, head);

    /* reset the head */
    head = GGC_RP(from, head);
    GGC_WP(to, head, head);

    /* add the length */
    tolen += fromlen;
    GGC_WD(to, length, tolen);

    /* and reset the old list */
    GGC_WD(from, length, 0);
    head = NULL;
    GGC_WP(from, head, head);
    GGC_WP(from, tail, head);

    return;
}

/* pop an element from the beginning of a generic list */
void *GGC_ListShift(GGC_List list)
{
    GGC_ListNode node = NULL, head = NULL;
    ggc_size_t len;

    GGC_PUSH_2(list, node);

    node = GGC_RP(list, head);
    if (node) {
        head = GGC_RP(node, next);
        GGC_WP(list, head, head);
        if (!head)
            GGC_WP(list, tail, head);

        len = GGC_RD(list, length) - 1;
        GGC_WD(list, length, len);
    }

    return node;
}

/* insert an element after the specified one, in the given list */
void GGC_ListInsertAfter(GGC_List list, GGC_ListNode after, void *value)
{
    GGC_ListNode node = NULL, next = NULL;
    ggc_size_t len;

    GGC_PUSH_5(list, after, value, node, next);

    node = GGC_NEW(GGC_ListNode);
    GGC_WP(node, el, value);

    next = GGC_RP(after, next);
    GGC_WP(node, next, next);
    GGC_WP(after, next, node);

    len = GGC_RD(list, length) + 1;
    GGC_WD(list, length, len);

    return;
}

/* insert a list after the specified element, in the given list */
void GGC_ListInsertAfterList(GGC_List to, GGC_ListNode after, GGC_List from)
{
    GGC_ListNode head = NULL, tail = NULL, next = NULL;
    ggc_size_t tolen, fromlen;

    GGC_PUSH_6(to, after, from, head, tail, next);

    /* pull them out of the old list */
    fromlen = GGC_RD(from, length);
    head = GGC_RP(from, head);
    tail = GGC_RP(from, tail);
    GGC_WD(from, length, 0);
    next = NULL;
    GGC_WP(from, head, next);
    GGC_WP(from, tail, next);

    /* graft them into the new list */
    next = GGC_RP(after, next);
    GGC_WP(after, next, head);
    GGC_WP(tail, next, next);

    /* then set the length properly */
    tolen = GGC_RD(to, length) + fromlen;
    GGC_WD(to, length, tolen);

    return;
}

/* convert a list to an array */
GGC_voidpArray GGC_ListToArray(GGC_List list)
{
    GGC_voidpArray ret = NULL;
    GGC_ListNode curn = NULL;
    void *cur = NULL;
    size_t i = 0;

    GGC_PUSH_4(list, ret, curn, cur);
    ret = GGC_NEW_PA(GGC_voidp, GGC_RD(list, length));

    for (i = 0, curn = GGC_RP(list, head);
         i < GGC_RD(list, length) && curn;
         i++, curn = GGC_RP(curn, next)) {
         cur = GGC_RP(curn, el);
         GGC_WAP(ret, i, cur);
    }

    return ret;
}
