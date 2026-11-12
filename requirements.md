Library Specifications:

Your shared library must provide the following function:

int crawl(
    const char *start_url,
    int download_workers,
    int parse_workers,
    int work_queue_size,
    char *(*fetch_fn)(const char *link),
    void (*edge_fn)(const char *from, const char *to));
crawl() will return 0 on success and -1 on failure.

The arguments are as follows:

- start_url: the first link to visit. All other links will be discovered by following links out recursively from this first page. (for the test cases, this will be the filename "root").
- download_workers: the number of worker threads in the download pool.
- parse_workers: the number of workers threads in the parse pool.
- work_queue_size: the size of the links queue (the downloaders pop off this queue to get more work).
- fetch_fn: a callback to ask the program using the library to fetch the content to which the link refers. Your download worker threads call this function. The function will return NULL if the content isn't reachable (a broken link). Otherwise, the function will malloc space for the return value, so you must free it when you're done.
- edge_fn: this is to notify the program that the library encountered a link on the from page to the to page.


Shared Library:

You must provide these routines in a shared library named "libcrawler.so". Placing the routines in a shared library instead of a simple object file makes it easier for other programmers to link with your code. There are further advantages to shared (dynamic) libraries over static libraries. When you link with a static library, the code for the entire library is merged with your object code to create your executable; if you link to many static libraries, your executable will be enormous. However, when you link to a shared library, the library's code is not merged with your program's object code; instead, a small amount of stub code is inserted into your object code and the stub code finds and invokes the library code when you execute the program. Therefore, shared libraries have two advantages: they lead to smaller executables and they enable users to use the most recent version of the library at run-time.

The provided Makefile builds the shared library for this project, though you are welcome to modify it as you see fit. When you submit, make libcrawler.so must build your code.

The Makefile will build the provided driver that calls your crawlmethod and serves files. It will also run your program. You may wish to generate other test cases, and you definitely want to experiment with different changes to the driver, like adding delay before returning the file.


Implementation Guidelines:

Your crawl() function should return when the entire graph has been visited, but we don't require you to cleanly exit from the thread pools.

You might want to use strstr() for parsing, although strsep() is a possibility, too. There are functions like strtok() you might find that have a separate interface, e.g. strtok_r(), that is reentrant, or thread safe (i.e., multiple threads can call it at the same time and nothing breaks). Be careful here and read the man page before calling a standard library function, because if you call a non-threadsafe function, you will have a hard time debugging your program.

There are many cycles in the web graph (e.g., A links to B, and B links to A). You need to make sure not to end up in an infinite loop. One way to avoid this is to use a hash table or hash set to keep track of already-processed links. You may copy a hash function from online (e.g., fletcher32 ), as long as a comment in your code references the source URL.

The parameters passed to the crawl function in the included driver.c file are just one possibility, you should test others.