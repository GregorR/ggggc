/*
 * GGGGC threads
 *
 * Copyright (C) 2011 Gregor Richards
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

/* for pthreads */
#define _POSIX_C_SOURCE 200112L

void GGC_threads_init_common();

/* get our real POSIX version */
#if defined(unix) || defined(__unix__) || defined(__unix)
#include <unistd.h>
#endif

#if _POSIX_VERSION >= 200112L /* should support pthreads */
#include "threads-pthreads.c"

#else
#include "threads-nothreads.c"

#endif

/* global thread identifier */
GGC_TLS(void *) GGC_thread_identifier;

/* common initialization */
void GGC_threads_init_common()
{
    GGC_TLS_INIT(GGC_thread_identifier);
    GGC_TLS_SET(GGC_thread_identifier, (void *) 0);
}
