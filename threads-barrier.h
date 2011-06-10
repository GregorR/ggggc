/*
 * GGGGC memory barrier support
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

/* The only exported #define is GGC_MEMORY_BARRIER() */

#ifndef GGGGC_THREADS_BARRIER_H
#define GGGGC_THREADS_BARRIER_H

#if defined(__GNUC__)
#define GGC_ARCH_BARRIER            __sync_synchronize()
#define GGC_COMP_BARRIER            __asm__ __volatile__ ("" ::: "memory")
#define GGC_FULL_BARRIER            __sync_synchronize()

#else
#define GGC_ARCH_BARRIER            do {} while (0)
#define GGC_COMP_BARRIER            do {} while (0)
#define GGC_FULL_BARRIER            do {} while (0)
#ifdef GGGGC_DEBUG_UNKNOWN_HOST
#warn Unknown host
#endif

#endif

#endif
