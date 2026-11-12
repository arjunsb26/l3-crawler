#ifndef PARSE_H
#define PARSE_H

/*
 * parse.h - pulling <a href="..."> targets out of a page body.
 *
 * This module is pure: no locks, no threads, no shared state. All iteration
 * state lives in the caller's link_iter, so any number of parser threads can
 * be in here at once.
 *
 * That is also why it uses strstr() and strchr() rather than strtok(): those
 * two are plain functions over the caller's buffer, with nothing saved
 * between calls. strtok() keeps hidden static state and two threads using it
 * concurrently would clobber each other. (strtok_r() would be safe, but there
 * is nothing here it would make simpler.)
 */

/* Internal to the library; see the note in queue.h. */
#pragma GCC visibility push(hidden)

/* Cursor over the links in a page body. Treat the field as opaque. */
typedef struct {
    const char *cursor;
} link_iter;

/*
 * Begin iterating over `body`, a NUL-terminated page.
 *
 * The iterator points into `body` rather than copying it, so `body` must stay
 * alive and unmodified for as long as the iterator is in use.
 */
void link_iter_init(link_iter *it, const char *body);

/*
 * Return the next link target, or NULL when the page holds no more. (An
 * allocation failure also reports NULL, ending iteration early; the caller
 * loses the rest of that page's links but nothing is corrupted.)
 *
 * The result is a malloc'd NUL-terminated string and THE CALLER OWNS IT --
 * either free() it or hand it to something that takes ownership.
 *
 * Only the href attribute is treated as a link. The corpus writes each link
 * as <a href="name">name</a>, and matching the visible anchor text too would
 * report every edge twice.
 */
char *link_iter_next(link_iter *it);

#pragma GCC visibility pop

#endif /* PARSE_H */
