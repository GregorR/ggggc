/*
 * Used to generate the incredibly-cumbersome ggggcpush.h
 *
 * Copyright (C) 2010 Gregor Richards
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>

#define PSTACK_MAX 20

int main()
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
    }
}
