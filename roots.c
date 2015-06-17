/*
 * Functions related to global roots
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "ggggc/gc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* globalize some local elements in the pointer stack */
void ggggc_globalize()
{
    struct GGGGC_PointerStack *gPointerStack;

    /* make a global copy */
    gPointerStack = (struct GGGGC_PointerStack *)
        malloc(sizeof(struct GGGGC_PointerStack) + ggggc_pointerStack->size * sizeof(void *));
    gPointerStack->next = NULL;
    gPointerStack->size = ggggc_pointerStack->size;
    memcpy(gPointerStack->pointers, ggggc_pointerStack->pointers, ggggc_pointerStack->size * sizeof(void *));

    /* then add it to the stack */
    if (!ggggc_pointerStackGlobals) {
        /* need to find the globals first! */
        struct GGGGC_PointerStack *cur = ggggc_pointerStack;
        while (cur->next) cur = cur->next;
        ggggc_pointerStackGlobals = cur;
    }
    ggggc_pointerStackGlobals->next = gPointerStack;
    ggggc_pointerStackGlobals = gPointerStack;
}

#ifdef __cplusplus
}
#endif
