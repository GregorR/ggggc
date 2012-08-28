/*
 * Initialization junk
 *
 * Copyright (c) 2010 Gregor Richards
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ggggc.h"
#include "ggggc_internal.h"
#include "helpers.h"

void GGGGC_init(void)
{
    int g;

    for (g = 0; g <= GGGGC_GENERATIONS; g++) {
        ggggc_gens[g] = GGGGC_alloc_pool();
    }
    ggggc_heurpool = ggggc_allocpool = ggggc_gens[0];
    ggggc_heurpoolmax = (char *) ggggc_heurpool + GGGGC_HEURISTIC_MAX;

    /* other inits */
    GGGGC_collector_init();
}
