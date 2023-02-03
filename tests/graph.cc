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

#include <cstdint>
#include <cstdio>

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

static void graph(GGCAP<Node, NodeArray> nodes)
{
    int i;
    GGCAP<Node, NodeArray> edges, nedges;
    GGC<Node> next, node;

#ifdef GGGGC_FEATURE_TAGGING
#define IS_TAGGED(p) ((ggc_size_t) (p.get()) & (sizeof(ggc_size_t)-1))
#else
#define IS_TAGGED(p) 0
#endif

    for (i = 0; i < ITERCT; i++) {
        /* first create a new link */
        ggc_size_t j = prand() % NODECT;
        node = nodes[j];
        if (!node || IS_TAGGED(node)) {
            node = GGC_NEW(Node);
            nodes.put(j, node);
#ifdef GGGGC_FEATURE_FINALIZERS
            outstanding++;
            GGC_FINALIZE(node, finalize);
#endif
        }
        edges = node->edges;
        if (!edges)
            edges = GGC_NEW_PA(Node, 1);

        j = prand() % NODECT;
        next = nodes[j];
        if (!next || IS_TAGGED(next)) {
            next = GGC_NEW(Node);
            nodes.put(j, next);
#ifdef GGGGC_FEATURE_FINALIZERS
            outstanding++;
            GGC_FINALIZE(next, finalize);
#endif
        }

        nedges = GGC_NEW_PA(Node, edges->length + 1);
        nedges.put(edges->length, next);
        for (j = 0; j < edges->length; j++) {
            next = edges[j];
            nedges.put(j, next);
        }
        node->edges = nedges;

        /* then delete an old node */
        j = prand() % NODECT;
        {
#ifndef GGGGC_FEATURE_TAGGING
            Node null = (Node) NULL;
#else
            Node null = (Node) (void *) (ggc_size_t) 1;
#endif
            nodes.put(j, null);
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
    GGCAP<Node, NodeArray> nodes{GGC_NEW_PA(Node, NODECT)};

    graph(nodes);

    for (ggc_size_t i = 0; i < nodes->length; i++)
        nodes.put(i, nullptr);

#ifdef GGGGC_FEATURE_FINALIZERS
    GGC_COLLECT();
    if (outstanding != 0) {
        fprintf(stderr, "%d outstanding finalizers!\n", outstanding);
        return 1;
    }
#endif

    return 0;
}
