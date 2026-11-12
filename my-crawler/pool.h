#ifndef POOL_H
#define POOL_H

#include <pthread.h>

/*
 * pool.h - a fixed-size pool of pthreads all running the same worker loop.
 *
 * Thin on purpose. The crawler needs two of these (downloaders and parsers)
 * and they differ only in the function they run and the context they get, so
 * the only thing worth factoring out is "spawn N of these, then join them."
 */
/* Internal to the library; see the note in queue.h. */
#pragma GCC visibility push(hidden)

typedef struct {
    pthread_t *threads;
    int count;                 /* threads actually created, hence joinable */
} thread_pool;

/*
 * Start `n` threads, each running fn(arg).
 *
 * Returns 0 if all `n` started. On failure returns -1, but the pool is still
 * left in a valid, joinable state describing however many threads did start,
 * so the caller can shut the queues down and pool_join() them rather than
 * abandoning running threads. Calling pool_join() is required either way.
 */
int pool_start(thread_pool *p, int n, void *(*fn)(void *), void *arg);

/*
 * Join every thread and release the pool's own memory.
 *
 * This blocks until the workers return, so whatever makes them return -- for
 * the crawler, stopping both queues -- has to happen first.
 */
void pool_join(thread_pool *p);

#pragma GCC visibility pop

#endif /* POOL_H */
