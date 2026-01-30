/*
 * WayMux: A Wayland multiplexer.
 *
 * Copyright (C) 2025 Ido Perlmuter
 *
 * See the LICENSE file accompanying this file.
 */

/*
 * Stubs for external dependencies of tab.c
 *
 * This file provides stubs for functions that tab.c depends on but which
 * have heavy dependencies (scene graph, rendering, etc.). The actual
 * tab functions (tab_count, tab_next, tab_prev, tab_set_background)
 * are tested by linking against the real tab.c implementation.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_scene.h>

#include "server.h"
#include "tab_bar.h"
#include "view.h"

/* Stub for view_activate called by tab_activate */
void
view_activate(struct cg_view *view, bool activated)
{
	(void)view;
	(void)activated;
	/* Stub - requires view system integration */
}

/* Stub for view_position called by tab_activate */
void
view_position(struct cg_view *view)
{
	(void)view;
	/* Stub - requires output and scene graph */
}

/* Stub for tab_bar_update called by tab_set_background and tab_create */
void
tab_bar_update(struct cg_tab_bar *tab_bar)
{
	(void)tab_bar;
	/* Stub - requires rendering system */
}

/* Stub for wlr_scene_tree_create called by tab_create */
struct wlr_scene_tree *
wlr_scene_tree_create(struct wlr_scene_tree *parent)
{
	(void)parent;
	/* Return a dummy allocation - tests won't actually use it */
	struct wlr_scene_tree *tree = calloc(1, sizeof(struct wlr_scene_tree));
	return tree;
}

/* Stub for wlr_scene_node_set_enabled called by tab_create and tab_activate */
void
wlr_scene_node_set_enabled(struct wlr_scene_node *node, bool enabled)
{
	(void)node;
	(void)enabled;
	/* Stub - scene graph manipulation */
}

/* Stub for wlr_scene_node_destroy called by tab_destroy */
void
wlr_scene_node_destroy(struct wlr_scene_node *node)
{
	if (node) {
		free(node);
	}
}
