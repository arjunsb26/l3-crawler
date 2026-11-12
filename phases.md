## Phase 1: Core Data Structures (Queues & Visited Trackers)

- Implement a bounded (fixed-size) queue to pass newly discovered links from parsers to downloaders.
- Equip this bounded queue with one mutex and two condition variables to handle the standard producer/consumer waiting states.
- Implement an unbounded queue to pass fetched web pages from downloaders to parsers.
- Use malloc for each new entry in the unbounded queue so that it never reaches a "full" state, avoiding the need for a "queue full" condition variable.
- Build a hash table or hash set to track already-processed links so your crawler avoids infinite loops in the graph.
- If you use an online hash function like fletcher32, include a comment in your code referencing the source URL.

## Phase 2: Thread Pools & Concurrency
- Set up your project to strictly use the pthreads library, explicitly utilizing pthread_create, pthread_cond_wait, pthread_cond_signal, and pthread_mutex_lock.
- Create the downloader thread pool, which is responsible for pulling tasks from the bounded queue and fetching page content.
- Create the parser thread pool, which is responsible for scanning downloaded pages from the unbounded queue.

## Phase 3: Main Crawler Logic & Callbacks
- Implement the main crawl function using the exact signature provided in the library specifications.
- In the downloader threads, call the fetch_fn callback to retrieve the page content for a given link.
- In the parser threads, search the fetched HTML strictly for links matching the <a href="filename">filename</a> format.
- When a new link is found, call the edge_fn callback to notify the driver program of the graph edge.
- Push this newly discovered link onto the downloaders' work queue.
- Ensure that all parsing logic utilizes thread-safe functions (like strtok_r instead of standard strtok) to prevent race conditions.
- Free the memory allocated by fetch_fn once your parsers are done processing the page.