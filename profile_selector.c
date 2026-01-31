/*
 * Waymux: Profile selector dialog
 *
 * Copyright (C) 2025 Ido Perlmuter
 *
 * See the LICENSE file accompanying this file.
 */

#include "profile_selector.h"
#include "output.h"
#include "pixel_buffer.h"
#include "server.h"
#include "profile.h"
#include "registry.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include <cairo/cairo.h>
#include <drm/drm_fourcc.h>
#include <wlr/interfaces/wlr_buffer.h>

/* Semi-transparent dark background (RGBA) */
static const float selector_bg_color[4] = {
	0.0f,  /* R */
	0.0f,  /* G */
	0.0f,  /* B */
	0.85f  /* A (85% opacity) */
};

/* Selector UI colors */
static const float selector_box_bg[4] = {0.12f, 0.12f, 0.12f, 1.0f};
static const float selector_selected_bg[4] = {0.22f, 0.33f, 0.44f, 1.0f};
static const float selector_text[4] = {1.0f, 1.0f, 1.0f, 1.0f};
static const float selector_query_bg[4] = {0.08f, 0.08f, 0.08f, 1.0f};

/* Get profiles.d directory path */
static char *
get_profiles_dir(void)
{
	const char *config_home = getenv("XDG_CONFIG_HOME");
	char *path = NULL;

	if (!config_home || config_home[0] == '\0') {
		const char *home = getenv("HOME");
		if (!home) {
			wlr_log(WLR_ERROR, "HOME environment variable not set");
			return NULL;
		}
		size_t len = strlen(home) + 30;
		path = malloc(len);
		if (!path) {
			wlr_log_errno(WLR_ERROR, "Failed to allocate path");
			return NULL;
		}
		snprintf(path, len, "%s/.config/waymux/profiles.d", home);
	} else {
		size_t len = strlen(config_home) + 20;
		path = malloc(len);
		if (!path) {
			wlr_log_errno(WLR_ERROR, "Failed to allocate path");
			return NULL;
		}
		snprintf(path, len, "%s/waymux/profiles.d", config_home);
	}

	return path;
}

/* Scan profiles directory for .toml files */
static int
scan_profiles(struct cg_profile_selector *selector)
{
	char *profiles_dir = get_profiles_dir();
	if (!profiles_dir) {
		return -1;
	}

	DIR *dir = opendir(profiles_dir);
	if (!dir) {
		wlr_log(WLR_DEBUG, "No profiles directory found: %s", profiles_dir);
		free(profiles_dir);
		return 0;
	}

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		/* Skip hidden files and non-TOML files */
		if (entry->d_name[0] == '.') {
			continue;
		}

		size_t len = strlen(entry->d_name);
		if (len < 6 || strcmp(entry->d_name + len - 5, ".toml") != 0) {
			continue;
		}

		/* Check if we have space for more profiles */
		if (selector->profile_count >= PROFILE_SELECTOR_MAX_PROFILES - 1) {
			wlr_log(WLR_ERROR, "Too many profiles, max %d", PROFILE_SELECTOR_MAX_PROFILES);
			break;
		}

		/* Create profile entry */
		struct cg_profile_entry *profile_entry = calloc(1, sizeof(struct cg_profile_entry));
		if (!profile_entry) {
			wlr_log_errno(WLR_ERROR, "Failed to allocate profile entry");
			continue;
		}

		/* Copy name without .toml extension */
		profile_entry->name = strndup(entry->d_name, len - 5);
		profile_entry->display_name = strdup(profile_entry->name);

		if (!profile_entry->name || !profile_entry->display_name) {
			wlr_log_errno(WLR_ERROR, "Failed to duplicate profile name");
			free(profile_entry->name);
			free(profile_entry->display_name);
			free(profile_entry);
			continue;
		}

		selector->profiles[selector->profile_count++] = profile_entry;
		wlr_log(WLR_DEBUG, "Found profile: %s", profile_entry->name);
	}

	closedir(dir);
	free(profiles_dir);

	wlr_log(WLR_INFO, "Found %zu profiles", selector->profile_count);
	return selector->profile_count;
}

/* Render selector UI to a buffer */
static struct wlr_buffer *
render_selector_ui(struct cg_profile_selector *selector, int screen_width, int screen_height)
{
	(void)screen_width;
	(void)screen_height;
	int box_width = 600;
	int box_height = 400;

	/* Only allocate buffer for the selector box area */
	size_t stride = box_width * 4;
	size_t size = box_height * stride;
	uint32_t *data = calloc(1, size);
	if (!data) {
		return NULL;
	}

	/* Create cairo surface */
	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		(unsigned char *)data, CAIRO_FORMAT_ARGB32, box_width, box_height, stride);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		free(data);
		return NULL;
	}

	cairo_t *cr = cairo_create(surface);
	if (!cr) {
		cairo_surface_destroy(surface);
		free(data);
		return NULL;
	}

	/* Draw selector box background */
	cairo_set_source_rgba(cr, selector_box_bg[0], selector_box_bg[1],
			    selector_box_bg[2], selector_box_bg[3]);
	cairo_rectangle(cr, 0, 0, box_width, box_height);
	cairo_fill(cr);

	/* Draw search box at top */
	int search_height = 50;
	cairo_set_source_rgba(cr, selector_query_bg[0], selector_query_bg[1],
			    selector_query_bg[2], selector_query_bg[3]);
	cairo_rectangle(cr, 0, 0, box_width, search_height);
	cairo_fill(cr);

	/* Draw search query text */
	cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
			      CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 14);
	cairo_set_source_rgb(cr, selector_text[0], selector_text[1], selector_text[2]);

	/* Draw query text with cursor */
	char query_display[PROFILE_SELECTOR_MAX_QUERY + 2];
	snprintf(query_display, sizeof(query_display), "%s|", selector->query);
	cairo_move_to(cr, 15, 30);
	cairo_show_text(cr, query_display);

	/* Draw results list */
	int results_y = search_height + 10;
	int item_height = 40;
	int max_items = (box_height - search_height - 20) / item_height;

	for (size_t i = 0; i < selector->result_count && i < (size_t)max_items; i++) {
		int item_y = results_y + i * item_height;

		/* Highlight selected item */
		if (i == selector->selected_index) {
			cairo_set_source_rgba(cr, selector_selected_bg[0],
					    selector_selected_bg[1],
					    selector_selected_bg[2],
					    selector_selected_bg[3]);
			cairo_rectangle(cr, 10, item_y, box_width - 20, item_height - 5);
			cairo_fill(cr);
		}

		/* Draw profile name */
		struct cg_profile_entry *entry = selector->results[i];
		cairo_set_source_rgb(cr, selector_text[0], selector_text[1], selector_text[2]);
		cairo_move_to(cr, 20, item_y + 25);

		/* Truncate name if too long */
		char name_display[100];
		snprintf(name_display, sizeof(name_display), "%s", entry->display_name);
		cairo_show_text(cr, name_display);
	}

	cairo_destroy(cr);
	cairo_surface_finish(surface);
	cairo_surface_destroy(surface);

	/* Create wlr_buffer wrapper */
	struct pixel_buffer *buffer = calloc(1, sizeof(*buffer));
	if (!buffer) {
		free(data);
		return NULL;
	}

	wlr_buffer_init(&buffer->base, &pixel_buffer_impl, box_width, box_height);
	buffer->data = data;
	buffer->width = box_width;
	buffer->height = box_height;
	buffer->size = size;

	return &buffer->base;
}

/* Update the rendered selector UI */
static void
selector_update_render(struct cg_profile_selector *selector)
{
	if (!selector->content_buffer || !selector->is_visible || !selector->dirty) {
		return;
	}

	/* Get output dimensions and position */
	struct cg_output *output;
	wl_list_for_each(output, &selector->server->outputs, link) {
		int screen_width = output->wlr_output->width;
		int screen_height = output->wlr_output->height;

		/* Calculate position for centered selector box */
		int box_width = 600;
		int box_height = 400;
		int box_x = (screen_width - box_width) / 2;
		int box_y = (screen_height - box_height) / 2;

		/* Position the content buffer at the correct location */
		wlr_scene_node_set_position(&selector->content_buffer->node,
					    box_x, box_y);

		/* Render new UI */
		struct wlr_buffer *new_buffer = render_selector_ui(selector,
								screen_width, screen_height);
		if (new_buffer) {
			wlr_scene_buffer_set_buffer(selector->content_buffer, new_buffer);
			wlr_buffer_drop(new_buffer);
		}

		selector->dirty = false;

		break; /* Use first output */
	}
}

/* Case-insensitive substring match */
static bool
profile_matches(const struct cg_profile_entry *profile, const char *query)
{
	if (!query || query[0] == '\0') {
		return true;
	}

	/* Convert both to lowercase for comparison */
	char profile_lower[256];
	char query_lower[PROFILE_SELECTOR_MAX_QUERY];

	size_t i;
	for (i = 0; i < strlen(profile->name) && i < sizeof(profile_lower) - 1; i++) {
		profile_lower[i] = tolower(profile->name[i]);
	}
	profile_lower[i] = '\0';

	for (i = 0; i < strlen(query) && i < sizeof(query_lower) - 1; i++) {
		query_lower[i] = tolower(query[i]);
	}
	query_lower[i] = '\0';

	return strstr(profile_lower, query_lower) != NULL;
}

/* Update filtered results based on current query */
static void
selector_update_results(struct cg_profile_selector *selector)
{
	selector->result_count = 0;
	selector->selected_index = 0;

	/* Add special "(no profile)" option at the top if query is empty */
	if (selector->query[0] == '\0') {
		/* The first profile entry is always the "(no profile)" option */
		if (selector->profile_count > 0 && selector->profiles[0]) {
			selector->results[selector->result_count++] = selector->profiles[0];
		}
	}

	/* Filter profiles based on query */
	for (size_t i = 0; i < selector->profile_count; i++) {
		/* Skip the "(no profile)" entry when doing regular matching */
		if (i == 0 && strcmp(selector->profiles[i]->name, "(no profile)") == 0) {
			continue;
		}

		if (profile_matches(selector->profiles[i], selector->query)) {
			if (selector->result_count < PROFILE_SELECTOR_MAX_PROFILES) {
				selector->results[selector->result_count++] = selector->profiles[i];
			}
		}
	}

	selector->dirty = true;
	selector_update_render(selector);
}

struct cg_profile_selector *
profile_selector_create(struct cg_server *server)
{
	struct cg_profile_selector *selector = calloc(1, sizeof(struct cg_profile_selector));
	if (!selector) {
		wlr_log(WLR_ERROR, "Failed to allocate profile selector");
		return NULL;
	}

	selector->server = server;
	selector->is_visible = false;
	selector->dirty = false;
	selector->query[0] = '\0';
	selector->query_len = 0;
	selector->result_count = 0;
	selector->selected_index = 0;
	selector->content_buffer = NULL;

	/* Create scene tree for selector overlay */
	selector->scene_tree = wlr_scene_tree_create(&server->scene->tree);
	if (!selector->scene_tree) {
		wlr_log(WLR_ERROR, "Failed to create profile selector scene tree");
		free(selector);
		return NULL;
	}

	/* Create background rectangle */
	selector->background = wlr_scene_rect_create(
		selector->scene_tree,
		100,  /* Initial width, will be resized */
		100,  /* Initial height, will be resized */
		selector_bg_color
	);
	if (!selector->background) {
		wlr_log(WLR_ERROR, "Failed to create profile selector background");
		wlr_scene_node_destroy(&selector->scene_tree->node);
		free(selector);
		return NULL;
	}

	/* Create special "(no profile)" entry */
	struct cg_profile_entry *no_profile = calloc(1, sizeof(struct cg_profile_entry));
	if (no_profile) {
		no_profile->name = strdup("(no profile)");
		no_profile->display_name = strdup("(no profile)");
		if (no_profile->name && no_profile->display_name) {
			selector->profiles[selector->profile_count++] = no_profile;
		} else {
			free(no_profile->name);
			free(no_profile->display_name);
			free(no_profile);
		}
	}

	/* Scan for available profiles */
	if (scan_profiles(selector) < 0) {
		wlr_log(WLR_ERROR, "Failed to scan profiles directory");
		/* Continue anyway - we still have the "(no profile)" option */
	}

	/* Initially hidden */
	wlr_scene_node_set_enabled(&selector->scene_tree->node, false);
	wlr_scene_node_raise_to_top(&selector->scene_tree->node);

	wlr_log(WLR_DEBUG, "Profile selector created with %zu profiles", selector->profile_count);
	return selector;
}

void
profile_selector_destroy(struct cg_profile_selector *selector)
{
	if (!selector) {
		return;
	}

	/* Free profile entries */
	for (size_t i = 0; i < selector->profile_count; i++) {
		if (selector->profiles[i]) {
			free(selector->profiles[i]->name);
			free(selector->profiles[i]->display_name);
			free(selector->profiles[i]);
		}
	}

	/* Scene tree cleanup is handled by wlroots when destroyed */
	wlr_scene_node_destroy(&selector->scene_tree->node);
	free(selector);

	wlr_log(WLR_DEBUG, "Profile selector destroyed");
}

void
profile_selector_show(struct cg_profile_selector *selector)
{
	if (!selector || selector->is_visible) {
		return;
	}

	/* Reset query and show all profiles */
	selector->query[0] = '\0';
	selector->query_len = 0;
	selector->selected_index = 0;

	/* Get the first output's dimensions */
	struct cg_output *output;
	wl_list_for_each(output, &selector->server->outputs, link) {
		struct wlr_output *wlr_output = output->wlr_output;
		int width = wlr_output->width;
		int height = wlr_output->height;

		/* Resize background to match output */
		wlr_scene_rect_set_size(selector->background, width, height);

		/* Create content buffer for UI if not exists */
		if (!selector->content_buffer) {
			selector->content_buffer =
				wlr_scene_buffer_create(selector->scene_tree, NULL);
		}

		break; /* Use first output for now */
	}

	wlr_scene_node_set_enabled(&selector->scene_tree->node, true);
	wlr_scene_node_raise_to_top(&selector->scene_tree->node);
	selector->is_visible = true;

	/* Update results and render (must be after is_visible is set) */
	selector_update_results(selector);

	wlr_log(WLR_DEBUG, "Profile selector shown");
}

void
profile_selector_hide(struct cg_profile_selector *selector)
{
	if (!selector || !selector->is_visible) {
		return;
	}

	wlr_scene_node_set_enabled(&selector->scene_tree->node, false);
	selector->is_visible = false;

	wlr_log(WLR_DEBUG, "Profile selector hidden");
}

/* Handle keyboard input when selector is visible */
bool
profile_selector_handle_key(struct cg_profile_selector *selector, xkb_keysym_t sym, uint32_t keycode)
{
	(void)keycode;
	if (!selector || !selector->is_visible) {
		return false;
	}

	bool handled = true;

	switch (sym) {
	case XKB_KEY_Escape:
		/* Close selector without selecting */
		profile_selector_hide(selector);
		break;

	case XKB_KEY_Return:
		/* Select the chosen profile */
		if (selector->result_count > 0 &&
		    selector->selected_index < selector->result_count) {
			struct cg_profile_entry *entry =
				selector->results[selector->selected_index];

			wlr_log(WLR_INFO, "Selected profile: %s", entry->name);

			/* Hide selector first */
			profile_selector_hide(selector);

			/* Check if "(no profile)" was selected */
			if (strcmp(entry->name, "(no profile)") == 0) {
				wlr_log(WLR_INFO, "Starting without a profile");
				/* Just hide selector, don't spawn anything */
			} else {
				/* Spawn the selected profile */
				if (!spawn_profile_tabs(selector->server, entry->name)) {
					wlr_log(WLR_ERROR, "Failed to spawn profile: %s", entry->name);
				}
			}
		}
		break;

	case XKB_KEY_BackSpace:
		/* Remove last character from query */
		if (selector->query_len > 0) {
			selector->query_len--;
			selector->query[selector->query_len] = '\0';
			selector_update_results(selector);
		} else {
			/* If query is empty, close selector */
			profile_selector_hide(selector);
		}
		break;

	case XKB_KEY_Up:
		/* Navigate up in results */
		if (selector->result_count > 0) {
			if (selector->selected_index > 0) {
				selector->selected_index--;
			} else {
				/* Wrap to bottom */
				selector->selected_index = selector->result_count - 1;
			}
			wlr_log(WLR_DEBUG, "Selected: %zu/%zu",
			        selector->selected_index, selector->result_count);
			selector->dirty = true;
			selector_update_render(selector);
		}
		break;

	case XKB_KEY_Down:
		/* Navigate down in results */
		if (selector->result_count > 0) {
			selector->selected_index++;
			if (selector->selected_index >= selector->result_count) {
				selector->selected_index = 0;  /* Wrap to top */
			}
			wlr_log(WLR_DEBUG, "Selected: %zu/%zu",
			        selector->selected_index, selector->result_count);
			selector->dirty = true;
			selector_update_render(selector);
		}
		break;

	default:
		/* Handle regular character input */
		if (sym >= XKB_KEY_space && sym <= XKB_KEY_asciitilde &&
		    selector->query_len < PROFILE_SELECTOR_MAX_QUERY - 1) {
			/* Convert keysym to character */
			char ch = (char)sym;
			selector->query[selector->query_len++] = ch;
			selector->query[selector->query_len] = '\0';
			selector_update_results(selector);
		} else {
			handled = false;
		}
		break;
	}

	return handled;
}
