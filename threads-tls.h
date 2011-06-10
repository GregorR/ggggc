/*
 * GGGGC TLS support
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

/* the exported #defines are:
 *
 * GGC_TLS(type):           Gives a TLS type for a pointer-sized type, e.g.
 *                          GGC_TLS(void *) myTlsPointer;
 * GGC_TLS_INIT(var):       Initializes the TLS variable var, if the host
 *                          platform requires such initialization.
 * GGC_TLS_GET(type, var):  Get a TLS variable var of type type.
 * GGC_TLS_SET(var, val):   Set a TLS variable var to value val.
 */

#ifndef GGGGC_THREADS_TLS_H
#define GGGGC_THREADS_TLS_H

/* figure out what the best kind of TLS to use is */
#if defined(__MACH__)
/* for now, no true TLS annotation on Mach-O :( */
#define GGGGC_TLS_DISPATCH

#elif defined(__GNUC__)
#define GGGGC_TLS_ANN __thread

#elif defined(_WIN32)
#define GGGGC_TLS_ANN __declspec(thread)

#elif defined(__SUNPRO_CC)
#define GGGGC_TLS_ANN __thread

#else
#define GGGGC_TLS_DISPATCH
#ifdef GGGGC_DEBUG_UNKNOWN_HOST
#warn Unknown host
#endif

#endif


/* now macros to create TLS variables */
#if defined(GGGGC_TLS_ANN)
#define GGC_TLS(type)           GGGGC_TLS_ANN type
#define GGC_TLS_INIT(var)
#define GGC_TLS_SET(var, val)   ((var) = (val))
#define GGC_TLS_GET(type, var)  ((type) (var))

#elif defined(GGGGC_TLS_DISPATCH)
#define GGC_TLS(type)           GGC_th_key_t
#define GGC_TLS_INIT(var)       do { \
                                    (var) = GGC_alloc_key(); \
                                    GGC_key_init(var); \
                                } while (0)
#define GGC_TLS_SET(var, val)   (GGC_key_set((var), (void *) (size_t) (val)))
#define GGC_TLS_GET(type, var)  ((type) (size_t) GGC_key_get(var))

#endif


#endif
