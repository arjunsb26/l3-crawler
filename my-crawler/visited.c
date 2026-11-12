#include <stdlib.h>
#include <string.h>

#include "visited.h"

/*
 * djb2 string hash, by Dan Bernstein.
 * Source: http://www.cse.yorku.ca/~oz/hash.html
 *
 * The accumulator is unsigned on purpose. Hashing into a signed int would
 * overflow (undefined behavior) and could produce a negative bucket index,
 * which is an out-of-bounds write that happens to be easy to miss.
 */
static unsigned long djb2(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    unsigned long hash = 5381;
    unsigned long c;

    while ((c = *p++) != 0)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

int visited_init(visited_set *v)
{
    if (!v)
        return -1;

    for (int i = 0; i < VISITED_BUCKETS; i++)
        v->buckets[i] = NULL;

    return pthread_mutex_init(&v->lock, NULL) == 0 ? 0 : -1;
}

void visited_destroy(visited_set *v)
{
    if (!v)
        return;

    for (int i = 0; i < VISITED_BUCKETS; i++) {
        visited_node *n = v->buckets[i];
        while (n) {
            visited_node *next = n->next;
            free(n->key);
            free(n);
            n = next;
        }
        v->buckets[i] = NULL;
    }
    pthread_mutex_destroy(&v->lock);
}

int visited_insert(visited_set *v, const char *url)
{
    if (!url)
        return -1;

    /* Hashing is pure, so keep it out of the critical section. */
    unsigned long idx = djb2(url) % VISITED_BUCKETS;
    int result;

    pthread_mutex_lock(&v->lock);

    result = 1;
    for (visited_node *n = v->buckets[idx]; n; n = n->next) {
        if (strcmp(n->key, url) == 0) {
            result = 0;
            break;
        }
    }

    if (result == 1) {
        visited_node *n = malloc(sizeof(*n));
        char *key = strdup(url);

        if (!n || !key) {
            free(n);
            free(key);
            result = -1;
        } else {
            n->key = key;
            n->next = v->buckets[idx];
            v->buckets[idx] = n;
        }
    }

    pthread_mutex_unlock(&v->lock);
    return result;
}
