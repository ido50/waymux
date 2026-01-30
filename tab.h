#ifndef CG_TAB_H
#define CG_TAB_H

#include "config.h"

#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>

#include "view.h"

struct cg_tab {
	struct cg_server *server;
	struct cg_view *view;
	struct wl_list link; // server::tabs

	/* Tab state */
	bool is_visible;
	bool is_background;  /* If true, tab is hidden from tab bar */

	/* Scene node for controlling visibility */
	struct wlr_scene_tree *scene_tree;
};

/* Create a new tab from a view */
struct cg_tab *tab_create(struct cg_server *server, struct cg_view *view);

/* Destroy a tab and close its view */
void tab_destroy(struct cg_tab *tab);

/* Set tab as active and visible */
void tab_activate(struct cg_tab *tab);

/* Set tab as background (hidden from tab bar) or foreground (visible in tab bar) */
void tab_set_background(struct cg_tab *tab, bool background);

/* Switch to the next tab (with wraparound) */
struct cg_tab *tab_next(struct cg_tab *current);

/* Switch to the previous tab (with wraparound) */
struct cg_tab *tab_prev(struct cg_tab *current);

/* Get the number of tabs */
int tab_count(struct cg_server *server);

/* Find tab by view */
struct cg_tab *tab_from_view(struct cg_view *view);

#endif
