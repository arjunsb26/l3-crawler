#ifndef API_H
#define API_H
/* 
 * crawl() - starts the crawling process
 *
 * The crawl function starts the crawling process. It creates two pools
 * of threads (downloaders and parsers) and manages their work queues.
 * The function returns when there is no more work to be done (all pages
 * reachable from start_url have been downloaded and parsed).
 *
 * Arguments:
 *   start_url: the first link to visit. All other links will be discovered
 *              by following links out recursively from this first page.
 *   download_workers: the number of worker threads in the download pool.
 *   parse_workers: the number of workers threads in the parse pool.
 *   work_queue_size: the size of the links queue (the downloaders pop off this
 *               queue to get more work).
 *   fetch_fn: a callback to ask the program using the library to fetch
 *             the content to which the link refers. Your download worker
 *             threads call this function. The function will return NULL
 *             if the content isn't reachable (a broken link). Otherwise,
 *             the function will malloc space for the return value, so you
 *             must free it when you're done.
 *   edge_fn: this is to notify the program that the library encountered a
 *            new edge in the crawl graph (from one page to another). The
 *            from and to arguments are the URLs of the two pages. The
 *            program using your library may use this information to build
 *            its own representation of the crawl graph.
 *
 * Returns:
 *   0 on success, negative value on error.
 */
int crawl(
    const char *start_url,
    int download_workers,
    int parse_workers,
    int work_queue_size,
    char *(*fetch_fn)(const char *link),
    void (*edge_fn)(const char *from, const char *to));
#endif /* API_H */
