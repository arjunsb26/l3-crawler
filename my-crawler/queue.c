#include <stdlib.h>

#include "queue.h"

/* ------------------------------------------------------------------ */
/* Bounded link queue                                                  */
/* ------------------------------------------------------------------ */

int lq_init(link_queue *q, int cap)
{
    /*
     * Rejecting cap < 1 is load-bearing, not just input hygiene. With a
     * capacity of zero the queue is simultaneously full and empty, so the
     * very first lq_submit() would wait for space that can never appear and
     * the whole crawler would deadlock on startup.
     */
    if (!q || cap < 1)
        return -1;

    q->slots = malloc((size_t)cap * sizeof(char *));
    if (!q->slots)
        return -1;

    q->cap = cap;
    q->count = 0;
    q->head = 0;
    q->tail = 0;
    q->stop = 0;
    q->pending = 0;

    if (pthread_mutex_init(&q->lock, NULL) != 0) {
        free(q->slots);
        return -1;
    }
    if (pthread_cond_init(&q->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&q->lock);
        free(q->slots);
        return -1;
    }
    if (pthread_cond_init(&q->not_full, NULL) != 0) {
        pthread_cond_destroy(&q->not_empty);
        pthread_mutex_destroy(&q->lock);
        free(q->slots);
        return -1;
    }
    if (pthread_cond_init(&q->all_done, NULL) != 0) {
        pthread_cond_destroy(&q->not_full);
        pthread_cond_destroy(&q->not_empty);
        pthread_mutex_destroy(&q->lock);
        free(q->slots);
        return -1;
    }
    return 0;
}

void lq_submit(link_queue *q, char *url)
{
    if (!url)
        return;

    pthread_mutex_lock(&q->lock);

    /*
     * Count the item before waiting for space. This is what guarantees a
     * parser parked on not_full is never mistaken for an idle crawler: it has
     * already counted this child and has not yet retired its parent page, so
     * pending is at least 2 and lq_wait_done() cannot be looking at zero.
     */
    q->pending++;

    while (q->count == q->cap && !q->stop)
        pthread_cond_wait(&q->not_full, &q->lock);

    if (q->stop) {
        /* Shutting down; drop the item rather than enqueue work nobody will take. */
        if (--q->pending == 0)
            pthread_cond_broadcast(&q->all_done);
        pthread_mutex_unlock(&q->lock);
        free(url);
        return;
    }

    q->slots[q->tail] = url;
    q->tail = (q->tail + 1) % q->cap;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

char *lq_pop(link_queue *q)
{
    pthread_mutex_lock(&q->lock);

    while (q->count == 0 && !q->stop)
        pthread_cond_wait(&q->not_empty, &q->lock);

    /*
     * Test emptiness, not `stop`: a stopped queue that still holds URLs
     * should hand them out rather than strand them.
     */
    if (q->count == 0) {
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }

    char *url = q->slots[q->head];
    q->head = (q->head + 1) % q->cap;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return url;
}

void lq_work_done(link_queue *q)
{
    pthread_mutex_lock(&q->lock);
    if (--q->pending == 0)
        pthread_cond_broadcast(&q->all_done);
    pthread_mutex_unlock(&q->lock);
}

void lq_wait_done(link_queue *q)
{
    pthread_mutex_lock(&q->lock);
    /*
     * A `while`, never an `if`. On a small graph the crawl can finish before
     * the main thread even reaches this point, and a broadcast with no waiter
     * is simply lost -- an `if` would then wait forever for a wakeup that
     * already happened. Re-testing the predicate also absorbs spurious wakeups.
     */
    while (q->pending != 0)
        pthread_cond_wait(&q->all_done, &q->lock);
    pthread_mutex_unlock(&q->lock);
}

void lq_stop(link_queue *q)
{
    /*
     * The flag is set while holding the same mutex the waiters evaluate their
     * predicates under. Setting it outside the lock would allow a worker to
     * check the predicate, then get preempted, then have us flip the flag and
     * broadcast into an empty room -- the worker would enter
     * pthread_cond_wait() afterwards and sleep forever, hanging pthread_join().
     * Marking the flag volatile would not help; the race is on the ordering of
     * the test and the wait, not on visibility.
     */
    pthread_mutex_lock(&q->lock);
    q->stop = 1;
    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->lock);
}

void lq_destroy(link_queue *q)
{
    if (!q)
        return;

    /*
     * A correct shutdown leaves this empty (pending == 0 implies both queues
     * are drained), but draining defensively turns a broken invariant into a
     * missing edge rather than a leak.
     */
    while (q->count > 0) {
        free(q->slots[q->head]);
        q->head = (q->head + 1) % q->cap;
        q->count--;
    }
    free(q->slots);
    q->slots = NULL;

    pthread_cond_destroy(&q->all_done);
    pthread_cond_destroy(&q->not_full);
    pthread_cond_destroy(&q->not_empty);
    pthread_mutex_destroy(&q->lock);
}

/* ------------------------------------------------------------------ */
/* Unbounded page queue                                                */
/* ------------------------------------------------------------------ */

int pq_init(page_queue *q)
{
    if (!q)
        return -1;

    q->head = NULL;
    q->tail = NULL;
    q->stop = 0;

    if (pthread_mutex_init(&q->lock, NULL) != 0)
        return -1;
    if (pthread_cond_init(&q->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&q->lock);
        return -1;
    }
    return 0;
}

int pq_push(page_queue *q, char *url, char *body)
{
    page_node *n = malloc(sizeof(*n));
    if (!n)
        return -1; /* caller still owns url and body */

    n->url = url;
    n->body = body;
    n->next = NULL;

    pthread_mutex_lock(&q->lock);
    if (q->tail)
        q->tail->next = n;
    else
        q->head = n;
    q->tail = n;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

int pq_pop(page_queue *q, char **url, char **body)
{
    pthread_mutex_lock(&q->lock);

    while (q->head == NULL && !q->stop)
        pthread_cond_wait(&q->not_empty, &q->lock);

    if (q->head == NULL) {
        pthread_mutex_unlock(&q->lock);
        return 0;
    }

    page_node *n = q->head;
    q->head = n->next;
    if (q->head == NULL)
        q->tail = NULL;
    pthread_mutex_unlock(&q->lock);

    /* Copy the payload out and drop the cell; the caller owns the strings now. */
    *url = n->url;
    *body = n->body;
    free(n);
    return 1;
}

void pq_stop(page_queue *q)
{
    pthread_mutex_lock(&q->lock); /* see lq_stop() for why this is locked */
    q->stop = 1;
    pthread_cond_broadcast(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

void pq_destroy(page_queue *q)
{
    if (!q)
        return;

    page_node *n = q->head;
    while (n) {
        page_node *next = n->next;
        free(n->url);
        free(n->body);
        free(n);
        n = next;
    }
    q->head = NULL;
    q->tail = NULL;

    pthread_cond_destroy(&q->not_empty);
    pthread_mutex_destroy(&q->lock);
}