#include <stdlib.h>
#include <string.h>

#include "parse.h"

/* The opening of a link, up to and including the quote around the target. */
static const char LINK_PREFIX[] = "<a href=\"";
#define LINK_PREFIX_LEN (sizeof(LINK_PREFIX) - 1)

void link_iter_init(link_iter *it, const char *body)
{
    it->cursor = body;
}

char *link_iter_next(link_iter *it)
{
    if (!it->cursor)
        return NULL;

    const char *open = strstr(it->cursor, LINK_PREFIX);
    if (!open) {
        it->cursor = NULL;
        return NULL;
    }

    const char *start = open + LINK_PREFIX_LEN;
    const char *close = strchr(start, '"');
    if (!close) {
        /* Truncated markup: an opening quote with nothing closing it. */
        it->cursor = NULL;
        return NULL;
    }

    /*
     * Resume past the closing quote. Advancing only past `open` would find
     * this same link again, and not advancing at all would spin forever.
     */
    it->cursor = close + 1;

    size_t len = (size_t)(close - start);
    char *target = malloc(len + 1);
    if (!target)
        return NULL;

    /*
     * memcpy plus an explicit NUL rather than strncpy: strncpy into an
     * exactly-sized buffer trips -Wstringop-truncation, which -Wall turns on
     * and -Werror turns fatal. Copying out also leaves `body` untouched,
     * instead of writing a NUL over the page's own closing quote.
     */
    memcpy(target, start, len);
    target[len] = '\0';
    return target;
}
