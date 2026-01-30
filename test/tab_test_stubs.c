/*
 * WayMux: A Wayland multiplexer.
 *
 * Copyright (C) 2025 Ido Perlmuter
 *
 * See the LICENSE file accompanying this file.
 */

/*
 * This file contains testable implementations of tab functions
 * that don't require the full WayMux infrastructure.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <wayland-server-core.h>

#include "server.h"
#include "tab.h"

int
tab_count(struct cg_server *server)
{
	if (!server) {
		return 0;
	}

	int count = 0;
	struct cg_tab *tab;
	wl_list_for_each(tab, &server->tabs, link) {
		count++;
	}
	return count;
}

/* Add stubs for other functions to avoid linker errors */
void
tab_activate(struct cg_tab *tab)
{
	(void)tab;
	/* Stub - not tested */
}

void
tab_set_background(struct cg_tab *tab, bool background)
{
	(void)tab;
	(void)background;
	/* Stub - not tested */
}

struct cg_tab *
tab_create(struct cg_server *server, struct cg_view *view)
{
	(void)server;
	(void)view;
	return NULL;
}

void
tab_destroy(struct cg_tab *tab)
{
	(void)tab;
	/* Stub - not tested */
}

struct cg_tab *
tab_next(struct cg_tab *current)
{
	if (!current) {
		return NULL;
	}

	struct cg_server *server = current->server;
	struct cg_tab *next = NULL;
	struct cg_tab *start = current;

	/* Get next tab in list, with wraparound */
	if (current->link.next != &server->tabs) {
		next = wl_container_of(current->link.next, next, link);
	} else {
		/* Wrap to first tab */
		next = wl_container_of(server->tabs.next, next, link);
	}

	/* Skip background tabs */
	while (next && next->is_background && next != start) {
		if (next->link.next != &server->tabs) {
			next = wl_container_of(next->link.next, next, link);
		} else {
			/* Wrap to first tab */
			next = wl_container_of(server->tabs.next, next, link);
		}
	}

	/* If we couldn't find a non-background tab (wrapped around to background only), return current */
	if (next && next->is_background) {
		return current;
	}

	return next;
}

struct cg_tab *
tab_prev(struct cg_tab *current)
{
	if (!current) {
		return NULL;
	}

	struct cg_server *server = current->server;
	struct cg_tab *prev = NULL;
	struct cg_tab *start = current;

	/* Get previous tab in list, with wraparound */
	if (current->link.prev != &server->tabs) {
		prev = wl_container_of(current->link.prev, prev, link);
	} else {
		/* Wrap to last tab */
		prev = wl_container_of(server->tabs.prev, prev, link);
	}

	/* Skip background tabs */
	while (prev && prev->is_background && prev != start) {
		if (prev->link.prev != &server->tabs) {
			prev = wl_container_of(prev->link.prev, prev, link);
		} else {
			/* Wrap to last tab */
			prev = wl_container_of(server->tabs.prev, prev, link);
		}
	}

	/* If we couldn't find a non-background tab (wrapped around to background only), return current */
	if (prev && prev->is_background) {
		return current;
	}

	return prev;
}

struct cg_tab *
tab_from_view(struct cg_view *view)
{
	if (!view || !view->server) {
		return NULL;
	}

	struct cg_tab *tab;
	wl_list_for_each(tab, &view->server->tabs, link) {
		if (tab->view == view) {
			return tab;
		}
	}
	return NULL;
}
