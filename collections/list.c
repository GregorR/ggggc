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
    size_t len;

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
