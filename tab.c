/*
 * WayMux: A Wayland multiplexer.
 *
 * Copyright (C) 2025 Ido Perlmuter
 * Based on Cage: Copyright (C) 2018-2021 Jente Hidskes
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "server.h"
#include "tab.h"
#include "view.h"

struct cg_tab *
tab_create(struct cg_server *server, struct cg_view *view)
{
	struct cg_tab *tab = calloc(1, sizeof(struct cg_tab));
	if (!tab) {
		wlr_log(WLR_ERROR, "Failed to allocate tab");
		return NULL;
	}

	tab->server = server;
	tab->view = view;
	tab->is_visible = false;

	/* Create a scene tree for this tab to control visibility */
	/* Note: server->scene is a wlr_scene, which has a built-in tree */
	tab->scene_tree = wlr_scene_tree_create(&server->scene->tree);
	if (!tab->scene_tree) {
		wlr_log(WLR_ERROR, "Failed to create tab scene tree");
		free(tab);
		return NULL;
	}

	/* Initially hide the tab */
	wlr_scene_node_set_enabled(&tab->scene_tree->node, false);

	/* Add to server's tab list */
	wl_list_insert(&server->tabs, &tab->link);

	wlr_log(WLR_DEBUG, "Created tab for view %p", (void *)view);
	return tab;
}

void
tab_destroy(struct cg_tab *tab)
{
	if (!tab) {
		return;
	}

	struct cg_server *server = tab->server;

	/* If this is the active tab, clear it */
	if (server->active_tab == tab) {
		server->active_tab = NULL;
	}

	/* Close the view */
	if (tab->view) {
		tab->view->impl->close(tab->view);
	}

	/* Destroy scene tree */
	if (tab->scene_tree) {
		wlr_scene_node_destroy(&tab->scene_tree->node);
	}

	/* Remove from list */
	wl_list_remove(&tab->link);

	wlr_log(WLR_DEBUG, "Destroyed tab");
	free(tab);
}

void
tab_activate(struct cg_tab *tab)
{
	if (!tab) {
		return;
	}

	struct cg_server *server = tab->server;

	/* Deactivate previously active tab */
	if (server->active_tab && server->active_tab != tab) {
		struct cg_tab *old_tab = server->active_tab;
		old_tab->is_visible = false;
		if (old_tab->scene_tree) {
			wlr_scene_node_set_enabled(&old_tab->scene_tree->node, false);
		}
		if (old_tab->view) {
			view_activate(old_tab->view, false);
		}
	}

	/* Activate new tab */
	tab->is_visible = true;
	server->active_tab = tab;

	if (tab->scene_tree) {
		wlr_scene_node_set_enabled(&tab->scene_tree->node, true);
	}

	if (tab->view) {
		view_activate(tab->view, true);
		view_position(tab->view);
	}

	wlr_log(WLR_DEBUG, "Activated tab %p", (void *)tab);
}

struct cg_tab *
tab_next(struct cg_tab *current)
{
	if (!current) {
		return NULL;
	}

	struct cg_server *server = current->server;
	struct cg_tab *next = NULL;

	/* Get next tab in list, with wraparound */
	if (current->link.next != &server->tabs) {
		next = wl_container_of(current->link.next, next, link);
	} else {
		/* Wrap to first tab */
		next = wl_container_of(server->tabs.next, next, link);
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

	/* Get previous tab in list, with wraparound */
	if (current->link.prev != &server->tabs) {
		prev = wl_container_of(current->link.prev, prev, link);
	} else {
		/* Wrap to last tab */
		prev = wl_container_of(server->tabs.prev, prev, link);
	}

	return prev;
}

int
tab_count(struct cg_server *server)
{
	int count = 0;
	struct cg_tab *tab;
	wl_list_for_each(tab, &server->tabs, link) {
		count++;
	}
	return count;
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
