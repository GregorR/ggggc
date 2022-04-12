/*
 * Thread support for Windows
 *
 * Copyright (c) 2014-2022 Gregor Richards
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

#ifndef GGGGC_THREADS_WINDOWS_H
#define GGGGC_THREADS_WINDOWS_H 1

/* functions */
#define ggc_mutex_unlock(mutex)         (!ReleaseMutex(*(mutex)))
#define ggc_sem_destroy(sem)            (!CloseHandle(*(sem)))
#define ggc_sem_init(sem,ct)            ((*(sem)=CreateSemaphore(NULL,0,(ct),NULL))?0:-1)
#define ggc_sem_post(sem)               (!ReleaseSemaphore(*(sem),1,NULL))
#define ggc_sem_wait_raw(sem)           (WaitForSingleObject(*(sem),INFINITE)==WAIT_FAILED)

/* types */
#define ggc_mutex_t     HANDLE
#define ggc_sem_t       HANDLE
#define ggc_thread_t    HANDLE

/* predefs */
#define GGC_MUTEX_INITIALIZER NULL

/* real code below */

/* functions */
int ggc_mutex_lock(ggc_mutex_t *mutex);
int ggc_mutex_lock_raw(ggc_mutex_t *mutex);
int ggc_mutex_trylock(ggc_mutex_t *mutex);
int ggc_sem_wait(ggc_sem_t *sem);
int ggc_thread_create(
        ggc_thread_t *thread,
        void (*func)(struct GGC_ThreadArg__ggggc_struct *),
        struct GGC_ThreadArg__ggggc_struct *arg);
int ggc_thread_join(ggc_thread_t thread);

#include "gen-barriers.h"

#endif
