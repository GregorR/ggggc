/*
 * Used to generate the incredibly-cumbersome ggggcpush.h
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

#include <stdio.h>

#define PSTACK_MAX 20

int main(void)
{
    int i, j;

    /* first the pointer stack primitives */
    for (i = 1; i <= 20; i++) {
        printf("struct GGGGC_PStack%d { void *next; void **ptrs[%d]; void *term; };\n", i, i);
    }

    for (i = 1; i <= 20; i++) {
        printf("#define GGC_PUSH%d(_obj1", i);
        for (j = 2; j <= i; j++)
            printf(", _obj%d", j);
        printf(") do { struct GGGGC_PStack%d *_ggggc_func_pstack = alloca(sizeof(struct GGGGC_PStack%d));"
               " _ggggc_func_pstack->next = ggggc_pstack;", i, i);
        for (j = 0; j < i; j++)
            printf(" _ggggc_func_pstack->ptrs[%d] = (void **) &(_obj%d);", j, j+1);
        printf(" _ggggc_func_pstack->term = NULL;"
               " ggggc_pstack = (void *) _ggggc_func_pstack; } while (0)\n");

        printf("#define GGC_DPUSH%d(_obj1", i);
        for (j = 2; j <= i; j++)
            printf(", _obj%d", j);
        printf(") do { struct GGGGC_PStack%d *_ggggc_func_pstack = alloca(sizeof(struct GGGGC_PStack%d));"
               " _ggggc_func_pstack->next = ggggc_dpstack;", i, i);
        for (j = 0; j < i; j++)
            printf(" _ggggc_func_pstack->ptrs[%d] = (void **) &(_obj%d);", j, j+1);
        printf(" _ggggc_func_pstack->term = NULL;"
               " ggggc_dpstack = (void *) _ggggc_func_pstack; } while (0)\n");
    }
}
