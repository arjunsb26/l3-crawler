#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>

/*
 * queue.h - the two work queues connecting the downloader and parser pools.
 *
 *   parsers --(link_queue, bounded)--> downloaders
 *   downloaders --(page_queue, unbounded)--> parsers
 *
 * The two pools feed each other, so this is a *cyclic* producer/consumer
 * system. That cycle is why the page queue is unbounded: if both queues were
 * bounded, parsers blocked on a full link queue and downloaders blocked on a
 * full page queue would wait on each other forever. Because a downloader can
 * never block while pushing, it always keeps draining the link queue, so a
 * parser blocked in lq_submit() is always eventually released.
 */

/*
 * Everything here is internal to the library. Hiding it keeps crawl() the
 * only symbol libcrawler.so exports, so these names cannot collide with
 * anything in the program that links against us.
 */
#pragma GCC visibility push(hidden)

/* ------------------------------------------------------------------ */
/* Bounded link queue (parsers -> downloaders)                         */
/* ------------------------------------------------------------------ */

/*
 * This struct also owns `pending`, the count of work items that exist
 * anywhere in the crawler: links sitting in this queue, pages sitting in the
 * page queue, and items currently in some worker's hands. The crawl is over
 * exactly when it reaches zero.
 *
 * Keeping the counter under the *same* mutex as the ring buffer is
 * deliberate, not laziness. lq_submit() counts a new item and enqueues it in
 * one critical section, so a parser blocked waiting for space has provably
 * already counted its child and not yet discounted its parent page. That
 * makes "pending can't hit zero while work remains" true by construction
 * rather than by remembering to call things in the right order.
 */
typedef struct {
    char **slots;             /* ring buffer of owned, malloc'd URL strings */
    int cap;                  /* capacity; must be >= 1 (see lq_submit)     */
    int count;                /* number of slots currently occupied         */
    int head;                 /* next slot to pop                           */
    int tail;                 /* next slot to fill                          */
    int stop;                 /* shutdown flag; ONLY touch while holding    */
                              /* `lock` -- see lq_stop()                    */
    long pending;             /* outstanding work items, crawler-wide       */
    pthread_mutex_t lock;
    pthread_cond_t not_empty; /* a popper waits here                        */
    pthread_cond_t not_full;  /* a pusher waits here                        */
    pthread_cond_t all_done;  /* lq_wait_done() waits here for pending == 0 */
} link_queue;

/* Returns 0 on success, -1 on bad capacity or allocation failure. */
int lq_init(link_queue *q, int cap);

/*
 * Free the ring and any URLs still in it, and destroy the sync primitives.
 * MUST NOT be called until every worker thread has been joined: destroying a
 * mutex that still has a waiter is undefined behavior.
 */
void lq_destroy(link_queue *q);

/*
 * Count a new work item and enqueue `url` for the downloaders. Blocks while
 * the queue is full. Takes ownership of `url` and frees it if the crawl is
 * already shutting down.
 */
void lq_submit(link_queue *q, char *url);

/*
 * Pop the next URL, blocking while the queue is empty. Returns NULL once the
 * queue is both stopped and drained, which is a worker's signal to exit.
 *
 * Does NOT change `pending`: the item merely moves from the queue into the
 * caller's hands, and it is still outstanding work either way.
 */
char *lq_pop(link_queue *q);

/*
 * Retire one work item. Call this when an item leaves the system for good --
 * a broken link, or a page that has been fully parsed. Wakes lq_wait_done()
 * when the count reaches zero.
 */
void lq_work_done(link_queue *q);

/* Block until `pending` reaches zero, i.e. until the whole crawl is done. */
void lq_wait_done(link_queue *q);

/* Wake every blocked worker so it can exit. Safe to call more than once. */
void lq_stop(link_queue *q);

/* ------------------------------------------------------------------ */
/* Unbounded page queue (downloaders -> parsers)                       */
/* ------------------------------------------------------------------ */

typedef struct page_node {
    char *url;                /* owned */
    char *body;               /* owned; came from fetch_fn */
    struct page_node *next;
} page_node;

/*
 * One node is malloc'd per entry, so this queue is never "full" and needs no
 * not_full condition variable and no blocking push.
 *
 * "Unbounded" is not as alarming as it sounds: the visited set guarantees
 * each URL is enqueued at most once, so this holds at most one node per
 * reachable page (1000 nodes and ~6 MB of page text for the largest corpus).
 */
typedef struct {
    page_node *head;
    page_node *tail;
    int stop;                 /* only touch while holding `lock` */
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
} page_queue;

/* Returns 0 on success, -1 on failure. */
int pq_init(page_queue *q);

/* Same join-before-destroy rule as lq_destroy(). */
void pq_destroy(page_queue *q);

/*
 * Hand a downloaded page to the parsers. Never blocks.
 *
 * On success returns 0 and takes ownership of both `url` and `body` -- the
 * caller must not touch either pointer afterwards. On allocation failure
 * returns -1 and takes ownership of neither.
 */
int pq_push(page_queue *q, char *url, char *body);

/*
 * Pop the next page, blocking while the queue is empty. On success returns 1
 * and stores the owned `url` and `body` pointers in the out-parameters.
 * Returns 0 once the queue is both stopped and drained.
 */
int pq_pop(page_queue *q, char **url, char **body);

/* Wake every blocked parser so it can exit. Safe to call more than once. */
void pq_stop(page_queue *q);

#pragma GCC visibility pop

#endif /* QUEUE_H */
