/*
 * Waymux: Profile selector dialog
 *
 * Copyright (C) 2025 Ido Perlmuter
 *
 * See the LICENSE file accompanying this file.
 */

#ifndef CG_PROFILE_SELECTOR_H
#define CG_PROFILE_SELECTOR_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include <xkbcommon/xkbcommon.h>

#define PROFILE_SELECTOR_MAX_QUERY 256
#define PROFILE_SELECTOR_MAX_PROFILES 256

struct cg_server;

/* Represents a discoverable profile */
struct cg_profile_entry {
	char *name;           /* Profile name (filename without .toml) */
	char *display_name;   /* Display name (could be enhanced later) */
};

struct cg_profile_selector {
	struct cg_server *server;
	struct wlr_scene_tree *scene_tree;
	struct wlr_scene_rect *background;
	struct wlr_scene_buffer *content_buffer;  /* Rendered selector UI */
	bool is_visible;
	bool dirty;  /* UI needs re-rendering */

	/* Search state */
	char query[PROFILE_SELECTOR_MAX_QUERY];
	size_t query_len;

	/* All discoverable profiles */
	struct cg_profile_entry *profiles[PROFILE_SELECTOR_MAX_PROFILES];
	size_t profile_count;

	/* Filtered results */
	struct cg_profile_entry *results[PROFILE_SELECTOR_MAX_PROFILES];
	size_t result_count;
	size_t selected_index;
};

struct cg_profile_selector *profile_selector_create(struct cg_server *server);
void profile_selector_destroy(struct cg_profile_selector *selector);
void profile_selector_show(struct cg_profile_selector *selector);
void profile_selector_hide(struct cg_profile_selector *selector);

/* Keyboard input handling */
/* Returns true if key was handled, false otherwise */
/* When user selects a profile, selector sets server->profile_name and hides itself */
bool profile_selector_handle_key(struct cg_profile_selector *selector, xkb_keysym_t sym, uint32_t keycode);

#endif
