#include <stdlib.h>
#include <string.h>

#include "crawler.h"
#include "parse.h"
#include "pool.h"

/* ------------------------------------------------------------------ */
/* Downloader pool                                                     */
/* ------------------------------------------------------------------ */

/*
 * Take a link, fetch its contents, hand the result to the parsers.
 *
 * Note what does *not* happen on the success path: the work item is not
 * retired. It has only moved from the link queue to the page queue, and it is
 * still outstanding work until a parser finishes with it.
 */
static void *downloader_loop(void *arg)
{
    crawler_ctx *c = arg;
    char *url;

    while ((url = lq_pop(&c->links)) != NULL) {
        char *body = c->fetch_fn(url);

        if (!body) {
            /* Broken link. Nothing further will come of it. */
            free(url);
            lq_work_done(&c->links);
            continue;
        }

        if (pq_push(&c->pages, url, body) != 0) {
            /*
             * Out of memory. Drop the page, but retire the item -- leaving it
             * counted would mean pending never reaches zero and crawl() would
             * never return. A lost page beats a hung crawler.
             */
            free(url);
            free(body);
            lq_work_done(&c->links);
            continue;
        }

        /* pq_push owns url and body now; touching either here is a double free. */
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Parser pool                                                         */
/* ------------------------------------------------------------------ */

static void *parser_loop(void *arg)
{
    crawler_ctx *c = arg;
    char *url;
    char *body;

    while (pq_pop(&c->pages, &url, &body)) {
        link_iter it;
        char *target;

        link_iter_init(&it, body);
        while ((target = link_iter_next(&it)) != NULL) {
            /*
             * Report every link occurrence, before and independently of the
             * visited check. The spec defines edge_fn as "the library
             * encountered a link on the from page to the to page", so a link
             * to an already-seen page is still an edge in the graph. Moving
             * this inside the branch below would under-report badly -- the
             * 1000-page corpus has 12896 link occurrences but only 999 pages
             * to discover.
             */
            pthread_mutex_lock(&c->edge_lock);
            c->edge_fn(url, target);
            pthread_mutex_unlock(&c->edge_lock);

            /*
             * The visited set decides who owns the job of crawling a URL.
             * Exactly one thread gets a 1 back for any given target, so the
             * page is queued exactly once no matter how many pages link to it.
             */
            if (visited_insert(&c->visited, target) == 1)
                lq_submit(&c->links, target); /* takes ownership of target */
            else
                free(target); /* already claimed, or we are out of memory */
        }

        free(body);
        free(url);

        /*
         * This MUST be the last thing the loop body does. Every child was
         * counted by lq_submit() above before this page was discounted here,
         * which is what keeps the outstanding-work count from touching zero
         * while there is still reachable work queued.
         */
        lq_work_done(&c->links);
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* crawl()                                                             */
/* ------------------------------------------------------------------ */

int crawl(
    const char *start_url,
    int download_workers,
    int parse_workers,
    int work_queue_size,
    char *(*fetch_fn)(const char *link),
    void (*edge_fn)(const char *from, const char *to))
{
    crawler_ctx c;
    thread_pool downloaders = { NULL, 0 };
    thread_pool parsers = { NULL, 0 };
    char *seed;
    int rc = 0;

    if (!start_url || !fetch_fn || !edge_fn ||
        download_workers < 1 || parse_workers < 1 || work_queue_size < 1)
        return -1;

    c.fetch_fn = fetch_fn;
    c.edge_fn = edge_fn;

    if (pthread_mutex_init(&c.edge_lock, NULL) != 0)
        return -1;
    if (lq_init(&c.links, work_queue_size) != 0)
        goto fail_edge_lock;
    if (pq_init(&c.pages) != 0)
        goto fail_links;
    if (visited_init(&c.visited) != 0)
        goto fail_pages;

    /*
     * Seed the crawl before any thread exists. Claiming the start URL here
     * means a page that links back to it is correctly treated as already
     * visited, and submitting here takes the outstanding-work count from 0 to
     * 1 -- if a worker could run first, it might find the count at zero and
     * conclude the crawl was already over.
     */
    seed = strdup(start_url);
    if (!seed)
        goto fail_visited;
    if (visited_insert(&c.visited, seed) != 1) {
        free(seed);
        goto fail_visited;
    }
    lq_submit(&c.links, seed); /* takes ownership of seed */

    if (pool_start(&downloaders, download_workers, downloader_loop, &c) != 0 ||
        pool_start(&parsers, parse_workers, parser_loop, &c) != 0)
        rc = -1;

    /*
     * Wait for the crawl to go quiet. If a pool failed to start we skip
     * straight to shutdown, but we still have to stop and join whatever
     * threads did start before tearing their shared state down.
     */
    if (rc == 0)
        lq_wait_done(&c.links);

    /*
     * Shutdown order matters. Stopping both queues wakes every parked worker
     * so it can return; only once they have all been joined is it safe to
     * free the queues and destroy the mutexes and condition variables they
     * were waiting on.
     */
    lq_stop(&c.links);
    pq_stop(&c.pages);
    pool_join(&downloaders);
    pool_join(&parsers);

    visited_destroy(&c.visited);
    pq_destroy(&c.pages);
    lq_destroy(&c.links);
    pthread_mutex_destroy(&c.edge_lock);
    return rc;

    /* Setup failed before any thread started; unwind what was built. */
fail_visited:
    visited_destroy(&c.visited);
fail_pages:
    pq_destroy(&c.pages);
fail_links:
    lq_destroy(&c.links);
fail_edge_lock:
    pthread_mutex_destroy(&c.edge_lock);
    return -1;
}
