#ifndef CRAWLER_H
#define CRAWLER_H

#include <pthread.h>

#include "../os-crawler-framework/api.h" /* not on the include path, hence the relative path */
#include "queue.h"
#include "visited.h"

/*
 * crawler.h - the state one crawl() call shares with its worker threads.
 *
 * This is a local inside crawl() rather than a global, so a program could run
 * two crawls at once without them colliding. Every worker gets a pointer to
 * the same instance.
 */
typedef struct {
    link_queue links;       /* parsers -> downloaders (bounded) */
    page_queue pages;       /* downloaders -> parsers (unbounded) */
    visited_set visited;    /* URLs already claimed by some thread */

    char *(*fetch_fn)(const char *link);
    void (*edge_fn)(const char *from, const char *to);

    /*
     * edge_fn belongs to the calling program and we have no idea whether it
     * is thread safe, so calls to it are serialized. (The supplied driver
     * only printf()s, which stdio already locks, but a different driver might
     * be building a graph.)
     */
    pthread_mutex_t edge_lock;
} crawler_ctx;

#endif /* CRAWLER_H */
