/*
 * Copyright (C) 2009, 2010 Gregor Richards
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

#ifndef BUFFER_H
#define BUFFER_H

#ifdef BUFFER_GC
#define _BUFFER_MALLOC GC_malloc
#define _BUFFER_REALLOC GC_realloc
#define _BUFFER_FREE GC_free

#else
#define _BUFFER_MALLOC malloc
#define _BUFFER_REALLOC realloc
#define _BUFFER_FREE free

#endif

#include "helpers.h"

#define BUFFER_DEFAULT_SIZE 1024

/* auto-expanding buffer */
#define BUFFER(name, type) \
struct Buffer_ ## name { \
    size_t bufsz, bufused; \
    type *buf; \
}

BUFFER(char, char);
BUFFER(int, int);

/* initialize a buffer */
#define INIT_BUFFER(buffer) \
{ \
    (buffer).bufsz = BUFFER_DEFAULT_SIZE; \
    (buffer).bufused = 0; \
    SF((buffer).buf, _BUFFER_MALLOC, NULL, (BUFFER_DEFAULT_SIZE * sizeof(*(buffer).buf))); \
}

/* free a buffer */
#define FREE_BUFFER(buffer) \
{ \
    if ((buffer).buf) _BUFFER_FREE((buffer).buf); \
    (buffer).buf = NULL; \
}

/* the address of the free space in the buffer */
#define BUFFER_END(buffer) ((buffer).buf + (buffer).bufused)

/* mark new bytes in a buffer */
#define STEP_BUFFER(buffer, by) ((buffer).bufused += by)

/* the amount of space left in the buffer */
#define BUFFER_SPACE(buffer) ((buffer).bufsz - (buffer).bufused)

/* expand a buffer */
#define EXPAND_BUFFER(buffer) \
{ \
    (buffer).bufsz *= 2; \
    SF((buffer).buf, _BUFFER_REALLOC, NULL, ((buffer).buf, (buffer).bufsz * sizeof(*(buffer).buf))); \
}

/* write a string to a buffer */
#define WRITE_BUFFER(buffer, string, len) \
{ \
    size_t _len = (len); \
    while (BUFFER_SPACE(buffer) < _len) { \
        EXPAND_BUFFER(buffer); \
    } \
    memcpy(BUFFER_END(buffer), string, _len * sizeof(*(buffer).buf)); \
    STEP_BUFFER(buffer, _len); \
}

/* read a file into a buffer */
#define READ_FILE_BUFFER(buffer, fh) \
{ \
    size_t _rd; \
    while ((_rd = fread(BUFFER_END(buffer), 1, BUFFER_SPACE(buffer), (fh))) > 0) { \
        STEP_BUFFER(buffer, _rd); \
        if (BUFFER_SPACE(buffer) <= 0) { \
            EXPAND_BUFFER(buffer); \
        } \
    } \
}

#endif
