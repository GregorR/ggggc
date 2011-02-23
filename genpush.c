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

int main()
{
    int i, j;
    for (i = 1; i <= 20; i++) {
        printf("#define GGC_PUSH%d(_obj1", i);
        for (j = 2; j <= i; j++)
            printf(", _obj%d", j);
        printf(") do { if (ggggc_pstack->rem < %d) GGGGC_pstackExpand(%d); ggggc_pstack->rem -= %d;", i, i, i);
        for (j = 1; j <= i; j++)
            printf(" *(ggggc_pstack->cur++) = (void **) &(_obj%d);", j);
        printf("} while(0)\n");
    }
}
