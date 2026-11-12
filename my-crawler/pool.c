#include <stdlib.h>

#include "pool.h"

int pool_start(thread_pool *p, int n, void *(*fn)(void *), void *arg)
{
    if (!p || n < 1)
        return -1;

    p->threads = malloc((size_t)n * sizeof(pthread_t));
    p->count = 0;
    if (!p->threads)
        return -1;

    for (int i = 0; i < n; i++) {
        if (pthread_create(&p->threads[i], NULL, fn, arg) != 0)
            return -1; /* p->count reflects what started; still joinable */
        p->count++;
    }
    return 0;
}

void pool_join(thread_pool *p)
{
    if (!p || !p->threads)
        return;

    for (int i = 0; i < p->count; i++)
        pthread_join(p->threads[i], NULL);

    free(p->threads);
    p->threads = NULL;
    p->count = 0;
}
