#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>

#include "api.h"

// Fetch the entire contents of file `name` located in directory `dir`.
// Returns a malloc'ed null-terminated buffer containing the file contents.
// Caller must free() the returned pointer.
// Returns NULL on error.
char* fetch(const char* dir, const char* name) {
    if (!dir || !name) {
        fprintf(stderr, "fetch: invalid arguments\n");
        return NULL;
    }

    // Construct full path: dir + "/" + name
    size_t len_dir = strlen(dir);
    size_t len_name = strlen(name);
    size_t len_path = len_dir + 1 + len_name + 1;
    char* path = malloc(len_path);
    if (!path) {
        perror("malloc");
        return NULL;
    }
    snprintf(path, len_path, "%s/%s", dir, name);

    FILE* f = fopen(path, "rb");
    free(path);
    if (!f) {
        fprintf(stderr, "fetch: cannot open file '%s/%s': %s\n",
                dir, name, strerror(errno));
        return NULL;
    }

    // Determine file size
    if (fseek(f, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(f);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0) {
        perror("ftell");
        fclose(f);
        return NULL;
    }
    rewind(f);

    // Allocate buffer (+1 for null terminator)
    char* buf = malloc((size_t)size + 1);
    if (!buf) {
        perror("malloc");
        fclose(f);
        return NULL;
    }

    // Read file contents
    size_t nread = fread(buf, 1, (size_t)size, f);
    if (nread != (size_t)size) {
        fprintf(stderr, "fetch: short read (expected %ld, got %zu)\n", size, nread);
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[size] = '\0'; // Null-terminate for convenience
    fclose(f);
    return buf;
}

const char* DEFAULT_TEST_DIR = "test3";
const char* test_dir = NULL;

// Upper bound, in microseconds, on the artificial delay fetch_dir() sleeps
// before returning a page. 0 disables it. A slow, variable fetch is what a
// real network looks like, and it widens the timing windows where a
// concurrency bug in the library would show up.
int fetch_delay_us = 0;

char* fetch_dir(const char* link) {
    if (fetch_delay_us > 0) {
        // rand_r() takes the caller's seed, so it is safe to call from
        // several downloader threads at once; plain rand() is not. Seeding
        // from the link address gives each page a different, stable delay.
        unsigned int seed = (unsigned int)(uintptr_t)link;
        usleep((useconds_t)(rand_r(&seed) % fetch_delay_us));
    }
    return fetch(test_dir, link);
}


// Print an edge "from:to" to stdout
void report_edge(const char* from, const char* to) {
    if (!from || !to) return;
    printf("edge %s:%s\n", from, to);
}


// usage: crawl [dir] [download_workers] [parse_workers] [queue_size] [delay_us]
//
// Every argument is optional and the defaults are the original ones, so
// running with no arguments (or just a directory) behaves exactly as before.
int main(int argc, char** argv) {
    const char* rootname = "root";

    int download_workers = 4;
    int parse_workers = 4;
    int queue_size = 10;

    if (argc >= 2 ) {
        test_dir = argv[1];
    } else {
        test_dir = DEFAULT_TEST_DIR;
    }

    if (argc >= 3) download_workers = atoi(argv[2]);
    if (argc >= 4) parse_workers    = atoi(argv[3]);
    if (argc >= 5) queue_size       = atoi(argv[4]);
    if (argc >= 6) fetch_delay_us   = atoi(argv[5]);

    // Configuration goes to stderr so that stdout stays a clean edge list
    // and can still be piped into sort/grep/wc.
    fprintf(stderr,
            "crawl: dir=%s downloaders=%d parsers=%d queue_size=%d delay<=%dus\n",
            test_dir, download_workers, parse_workers, queue_size, fetch_delay_us);

    int rc = crawl(
        rootname,          // start_url
        download_workers,  // download_workers
        parse_workers,     // parse_workers
        queue_size,        // queue_size
        fetch_dir,        // fetch_fn
        report_edge       // edge_fn
    );

    if (rc != 0) {
        fprintf(stderr, "crawl: returned %d\n", rc);
        return 1;
    }

    return 0;
}
