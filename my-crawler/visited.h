#ifndef VISITED_H
#define VISITED_H

#include <pthread.h>

/*
 * visited.h - the set of links the crawler has already claimed.
 *
 * The web graph has cycles (page A links to B, B links back to A), so without
 * this the crawler would follow them forever. Every URL is admitted exactly
 * once, by exactly one thread, and that thread is the one responsible for
 * queueing it.
 */

/* Internal to the library; see the note in queue.h. */
#pragma GCC visibility push(hidden)

/* Fixed bucket count. The largest corpus has 1000 pages, so chains average
 * about one node and there is no reason to grow the table at runtime. */
#define VISITED_BUCKETS 1024

typedef struct visited_node {
    char *key;                 /* owned by the set, not by the caller */
    struct visited_node *next;
} visited_node;

typedef struct {
    visited_node *buckets[VISITED_BUCKETS];
    pthread_mutex_t lock;
} visited_set;

/* Returns 0 on success, -1 on failure. */
int visited_init(visited_set *v);

/* Same join-before-destroy rule as the queues. */
void visited_destroy(visited_set *v);

/*
 * Atomically test whether `url` is in the set and add it if it is not.
 *
 * Returns 1 if this call is the one that added it (so the caller owns the job
 * of crawling it), 0 if some other thread got there first, and -1 on
 * allocation failure.
 *
 * The test and the insert have to happen under one lock. Exposing separate
 * "contains" and "add" calls would let two parsers both see a URL as absent
 * and both queue it, and the page would be downloaded twice.
 *
 * The set copies `url`; the caller keeps ownership of whatever it passed in.
 * Storing the caller's pointer instead would leave the table full of dangling
 * keys, because those strings are freed once their page has been parsed.
 */
int visited_insert(visited_set *v, const char *url);

#pragma GCC visibility pop

#endif /* VISITED_H */
