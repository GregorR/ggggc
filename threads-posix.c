int ggc_thread_create(ggc_thread_t *thread, void (*func)(ThreadArg), ThreadArg arg)
{
    ThreadInfo ti = NULL;

    GGC_PUSH_2(arg, ti);

    /* set up its thread info */
    ti = GGC_NEW(ThreadInfo);
    ti->func = func;
    GGC_W(ti, arg, arg);

    /* update our thread barrier */
    while (ggc_mutex_trylock(&ggggc_worldBarrierLock) != 0)
        GGC_YIELD();
    if (ggggc_threadCount == 0) ggggc_threadCount++;
    ggc_barrier_init(&ggggc_worldBarrier, NULL, ++ggggc_threadCount);
    ggc_mutex_unlock(&ggggc_worldBarrierLock);

    /* spawn the pthread */
    if ((errno = pthread_create(thread, NULL, ggggcThreadWrapper, ti)))
        return -1;

    return 0;
}

int ggc_thread_join(ggc_thread_t thread)
{
    int ret, err;

    /* when we join a thread, we are effectively removing ourselves from GC
     * contention. First we need to make sure our pool is 100% clean */
    ggggc_collect(0);

    /* then we need to reduce the thread count */
    while (ggc_mutex_trylock(&ggggc_worldBarrierLock) != 0)
        GGC_YIELD();
    ggc_barrier_init(&ggggc_worldBarrier, NULL, --ggggc_threadCount);
    ggc_mutex_unlock(&ggggc_worldBarrierLock);

    /* now we can actually join */
    err = pthread_join(thread, NULL);
    ret = err ? -1 : 0;

    /* then we rejoin the thread pool */
    while (ggc_mutex_trylock(&ggggc_worldBarrierLock) != 0)
        GGC_YIELD();
    ggc_barrier_init(&ggggc_worldBarrier, NULL, ++ggggc_threadCount);
    ggc_mutex_unlock(&ggggc_worldBarrierLock);

    errno = err;
    return ret;
}
