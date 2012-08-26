/*
 * Global variables (all one of them)
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

#include "ggggc.h"

struct GGGGC_Pool *ggggc_gens[GGGGC_GENERATIONS+1];
struct GGGGC_Pool *ggggc_heurpool = NULL, *ggggc_allocpool = NULL;
char *ggggc_heurpoolmax = NULL;
struct GGGGC_PStack *ggggc_pstack = NULL;
struct GGGGC_PStack *ggggc_dpstack = NULL;

size_t GGGGC_fytheConstBankPtrs = 0;
void *GGGGC_fytheConstBank = NULL;
void *GGGGC_fytheStackBase = NULL;
void *GGGGC_fytheStackTop = NULL;
