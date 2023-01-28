/*
 * Simple graph generation as a stress test and feature test
 *
 * Copyright (c) 2023 Gregor Richards
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

#include <stdint.h>
#include <stdio.h>

GGC_TYPE(Node)
    GGC_MPTR(NodeArray, edges);
GGC_END_TYPE(Node,
    GGC_PTR(Node, edges)
    )

static unsigned long seed = 1;

#define PRAND_MAX 32767
static int prand()
{
    seed = seed * 1103515245 + 12345;
    return (unsigned)(seed/65536) % 32768;
}

#define NODECT 1024
#define ITERCT (1024*1024)

#ifdef GGGGC_FEATURE_FINALIZERS
static int outstanding = 0;

static void finalize(void *ignore)
{
    outstanding--;
}
#endif

static void graph()
{
    int i;

#ifndef GGGGC_FEATURE_JITPSTACK
    NodeArray nodes = NULL, edges = NULL, nedges = NULL;
    Node node = NULL, next = NULL;

    GGC_PUSH_5(nodes, edges, nedges, node, next);
#else
    struct GraphFrame {
#ifdef GGGGC_FEATURE_EXTTAG
        ggc_size_t tags;
#endif
        NodeArray nodes, edges, nedges;
        Node node;
#if defined(GGGGC_FEATURE_EXTTAG) && SIZE_MAX < 0x100000000ULL
        /* 32 bit */
        ggc_size_t tags2;
#endif
        Node next;
    };
    struct GraphFrame *frame;
    ggc_jitPointerStack = ggc_jitPointerStack - sizeof(struct GraphFrame) / sizeof(void *);
    frame = (struct GraphFrame *) ggc_jitPointerStack;
#ifdef GGGGC_FEATURE_EXTTAG
#if SIZE_MAX < 0x100000000ULL
    /* 32 bit */
    frame->tags = 0x06040200;
    frame->tags2 = 0xFF08;
#else
    /* 64 bit */
    frame->tags = 0xFF0806040200ULL;
#endif
#endif
#undef GGGGC_ASSERT_ID
#define GGGGC_ASSERT_ID(id) 0
#define nodes frame->nodes
#define edges frame->edges
#define nedges frame->nedges
#define node frame->node
#define next frame->next
#endif

    nodes = GGC_NEW_PA(Node, NODECT);

#ifdef GGGGC_FEATURE_TAGGING
#define IS_TAGGED(p) ((ggc_size_t) (p) & (sizeof(ggc_size_t)-1))
#else
#define IS_TAGGED(p) 0
#endif

    for (i = 0; i < ITERCT; i++) {
        /* first create a new link */
        int j = prand() % NODECT;
        node = GGC_RAP(nodes, j);
        if (!node || IS_TAGGED(node)) {
            node = GGC_NEW(Node);
            GGC_WAP(nodes, j, node);
#ifdef GGGGC_FEATURE_FINALIZERS
            outstanding++;
            GGC_FINALIZE(node, finalize);
#endif
        }
        edges = GGC_RP(node, edges);
        if (!edges)
            edges = GGC_NEW_PA(Node, 1);

        j = prand() % NODECT;
        next = GGC_RAP(nodes, j);
        if (!next || IS_TAGGED(next)) {
            next = GGC_NEW(Node);
            GGC_WAP(nodes, j, next);
#ifdef GGGGC_FEATURE_FINALIZERS
            outstanding++;
            GGC_FINALIZE(next, finalize);
#endif
        }

        nedges = GGC_NEW_PA(Node, edges->length + 1);
        GGC_WAP(nedges, edges->length, next);
        for (j = 0; j < edges->length; j++) {
            next = GGC_RAP(edges, j);
            GGC_WAP(nedges, j, next);
        }
        GGC_WP(node, edges, nedges);

        /* then delete an old node */
        j = prand() % NODECT;
        {
#ifndef GGGGC_FEATURE_TAGGING
            Node null = (Node) NULL;
#else
            Node null = (Node) (void *) (ggc_size_t) 1;
#endif
            GGC_WAP(nodes, j, null);
        }

#ifdef GGGGC_FEATURE_TAGGING
        node = (Node) (void *) (ggc_size_t) 0x12345679;
        next = (Node) (void *) (ggc_size_t) -1;
#endif

        if (i % 1000 == 0) {
            /* stress test it by forcing collection now and then */
            if (i % 10000 == 0)
                GGC_COLLECT();
            fprintf(stderr, "%d\r", i);
        }
    }
    fprintf(stderr, "%d\n", i);

#ifdef GGGGC_FEATURE_JITPSTACK
    ggc_jitPointerStack = ggc_jitPointerStackEnd;
#endif
}

int main()
{
#ifdef GGGGC_FEATURE_JITPSTACK
    ggc_jitPointerStack = ggc_jitPointerStackEnd =
        (void **) calloc(4096, sizeof(void *)) + 4096;
#endif

    graph();

#ifdef GGGGC_FEATURE_FINALIZERS
    GGC_COLLECT();
    if (outstanding != 0) {
        fprintf(stderr, "Outstanding finalizers!\n");
        return 1;
    }
#endif

    return 0;
}
