#include "background_dialog.h"
#include "output.h"
#include "server.h"
#include "tab.h"
#include "view.h"
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <cairo/cairo.h>
#include <drm/drm_fourcc.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/render/wlr_renderer.h>

/* Semi-transparent dark background (RGBA) */
static const float dialog_bg_color[4] = {
	0.0f,  /* R */
	0.0f,  /* G */
	0.0f,  /* B */
	0.85f  /* A (85% opacity) */
};

/* Dialog UI colors */
static const float dialog_box_bg[4] = {0.12f, 0.12f, 0.12f, 1.0f};  /* Dark box background */
static const float dialog_selected_bg[4] = {0.22f, 0.33f, 0.44f, 1.0f};  /* Selected item */
static const float dialog_text[4] = {1.0f, 1.0f, 1.0f, 1.0f};  /* White text */
static const float dialog_query_bg[4] = {0.08f, 0.08f, 0.08f, 1.0f};  /* Search box */

/* Custom buffer that wraps pixel data */
struct pixel_buffer {
	struct wlr_buffer base;
	uint32_t *data;
	int width;
	int height;
	size_t size;
};

static void
pixel_buffer_destroy(struct pixel_buffer *buffer)
{
	if (!buffer) return;
	if (buffer->data) {
		free(buffer->data);
	}
	wlr_buffer_finish(&buffer->base);
	free(buffer);
}

static bool
pixel_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
				    uint32_t flags, void **data_out,
				    uint32_t *format, size_t *stride)
{
	struct pixel_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	*data_out = buffer->data;
	*format = DRM_FORMAT_ARGB8888;
	*stride = buffer->width * 4;
	return true;
}

static void
pixel_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer)
{
	/* Nothing to do */
}

static const struct wlr_buffer_impl pixel_buffer_impl = {
	.destroy = (void*)pixel_buffer_destroy,
	.begin_data_ptr_access = pixel_buffer_begin_data_ptr_access,
	.end_data_ptr_access = pixel_buffer_end_data_ptr_access,
};

/* Check if a tab matches the search query */
static bool
tab_matches_query(struct cg_tab *tab, const char *query)
{
	if (query[0] == '\0') {
		return true;
	}

	if (!tab->view) {
		return false;
	}

	char *title = view_get_title(tab->view);
	if (!title) {
		return false;
	}

	/* Case-insensitive substring search */
	char query_lower[BACKGROUND_DIALOG_MAX_QUERY];
	snprintf(query_lower, sizeof(query_lower), "%s", query);
	for (size_t i = 0; query_lower[i]; i++) {
		query_lower[i] = tolower(query_lower[i]);
	}

	char title_lower[256];
	snprintf(title_lower, sizeof(title_lower), "%s", title);
	for (size_t i = 0; title_lower[i]; i++) {
		title_lower[i] = tolower(title_lower[i]);
	}

	bool matches = strstr(title_lower, query_lower) != NULL;
	free(title);
	return matches;
}

/* Update the filtered results list based on current query */
static void
background_dialog_update_results(struct cg_background_dialog *dialog)
{
	dialog->result_count = 0;

	/* Iterate over all tabs and filter background tabs that match query */
	struct cg_tab *tab;
	wl_list_for_each(tab, &dialog->server->tabs, link) {
		if (!tab->is_background) {
			continue;
		}

		if (!tab_matches_query(tab, dialog->query)) {
			continue;
		}

		if (dialog->result_count < 256) {
			dialog->results[dialog->result_count++] = tab;
		}
	}

	/* Reset selection if out of bounds */
	if (dialog->selected_index >= dialog->result_count) {
		dialog->selected_index = dialog->result_count > 0 ? dialog->result_count - 1 : 0;
	}

	dialog->dirty = true;
}

/* Render background dialog UI to a buffer */
static struct wlr_buffer *
render_dialog_ui(struct cg_background_dialog *dialog, int screen_width, int screen_height)
{
	/* Dialog box dimensions - only render the box, not the whole screen */
	(void)screen_width;
	(void)screen_height;
	int box_width = 600;
	int box_height = 400;

	/* Only allocate buffer for the dialog box area */
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

	/* Draw dialog box background */
	cairo_set_source_rgba(cr, dialog_box_bg[0], dialog_box_bg[1],
			    dialog_box_bg[2], dialog_box_bg[3]);
	cairo_rectangle(cr, 0, 0, box_width, box_height);
	cairo_fill(cr);

	/* Draw search box at top */
	int search_height = 50;
	cairo_set_source_rgba(cr, dialog_query_bg[0], dialog_query_bg[1],
			    dialog_query_bg[2], dialog_query_bg[3]);
	cairo_rectangle(cr, 0, 0, box_width, search_height);
	cairo_fill(cr);

	/* Draw search query text */
	cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
			      CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 14);
	cairo_set_source_rgb(cr, dialog_text[0], dialog_text[1], dialog_text[2]);

	/* Draw query text with cursor */
	char query_display[BACKGROUND_DIALOG_MAX_QUERY + 2];
	snprintf(query_display, sizeof(query_display), "%s|", dialog->query);
	cairo_move_to(cr, 15, 30);
	cairo_show_text(cr, query_display);

	/* Draw results list */
	int results_y = search_height + 10;
	int item_height = 40;
	int max_items = (box_height - search_height - 20) / item_height;

	for (size_t i = 0; i < dialog->result_count && i < (size_t)max_items; i++) {
		int item_y = results_y + i * item_height;

		/* Highlight selected item */
		if (i == dialog->selected_index) {
			cairo_set_source_rgba(cr, dialog_selected_bg[0],
					    dialog_selected_bg[1],
					    dialog_selected_bg[2],
					    dialog_selected_bg[3]);
			cairo_rectangle(cr, 10, item_y, box_width - 20, item_height - 5);
			cairo_fill(cr);
		}

		/* Draw tab title */
		struct cg_tab *tab = dialog->results[i];
		cairo_set_source_rgb(cr, dialog_text[0], dialog_text[1], dialog_text[2]);
		cairo_move_to(cr, 20, item_y + 25);

		/* Get title and truncate if too long */
		char *title = tab->view ? view_get_title(tab->view) : NULL;
		char title_display[100];
		snprintf(title_display, sizeof(title_display), "%s",
			 title ? title : "<Untitled>");
		cairo_show_text(cr, title_display);
		free(title);
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

/* Update the rendered dialog UI */
static void
background_dialog_update_render(struct cg_background_dialog *dialog)
{
	if (!dialog->content_buffer || !dialog->is_visible || !dialog->dirty) {
		return;
	}

	/* Get output dimensions and position */
	struct cg_output *output;
	wl_list_for_each(output, &dialog->server->outputs, link) {
		int screen_width = output->wlr_output->width;
		int screen_height = output->wlr_output->height;

		/* Calculate position for centered dialog box */
		int box_width = 600;
		int box_height = 400;
		int x = (screen_width - box_width) / 2;
		int y = (screen_height - box_height) / 2;

		/* Render dialog UI */
		struct wlr_buffer *buffer = render_dialog_ui(dialog, screen_width, screen_height);
		if (!buffer) {
			wlr_log(WLR_ERROR, "Failed to render background dialog UI");
			return;
		}

		/* Update scene buffer */
		wlr_scene_buffer_set_buffer(dialog->content_buffer, buffer);
		wlr_scene_node_set_position(&dialog->content_buffer->node, x, y);

		/* Release old buffer reference */
		wlr_buffer_drop(buffer);

		break;  /* Only render on first output */
	}

	dialog->dirty = false;
}

struct cg_background_dialog *
background_dialog_create(struct cg_server *server)
{
	struct cg_background_dialog *dialog = calloc(1, sizeof(*dialog));
	if (!dialog) {
		wlr_log(WLR_ERROR, "Failed to allocate background dialog");
		return NULL;
	}

	dialog->server = server;
	dialog->is_visible = false;
	dialog->dirty = false;
	dialog->query[0] = '\0';
	dialog->query_len = 0;
	dialog->result_count = 0;
	dialog->selected_index = 0;

	/* Create scene tree for dialog */
	dialog->scene_tree = wlr_scene_tree_create(&server->scene->tree);
	if (!dialog->scene_tree) {
		wlr_log(WLR_ERROR, "Failed to create dialog scene tree");
		free(dialog);
		return NULL;
	}

	/* Create fullscreen background (semi-transparent overlay) */
	dialog->background = wlr_scene_rect_create(dialog->scene_tree, 0, 0, dialog_bg_color);
	if (!dialog->background) {
		wlr_log(WLR_ERROR, "Failed to create dialog background");
		wlr_scene_node_destroy(&dialog->scene_tree->node);
		free(dialog);
		return NULL;
	}

	/* Create content buffer for dialog UI */
	dialog->content_buffer = wlr_scene_buffer_create(dialog->scene_tree, NULL);
	if (!dialog->content_buffer) {
		wlr_log(WLR_ERROR, "Failed to create dialog content buffer");
		wlr_scene_node_destroy(&dialog->background->node);
		wlr_scene_node_destroy(&dialog->scene_tree->node);
		free(dialog);
		return NULL;
	}

	/* Initially hide the dialog */
	wlr_scene_node_set_enabled(&dialog->scene_tree->node, false);

	wlr_log(WLR_DEBUG, "Background dialog created");
	return dialog;
}

void
background_dialog_destroy(struct cg_background_dialog *dialog)
{
	if (!dialog) {
		return;
	}

	/* Scene tree and its children are destroyed automatically */
	free(dialog);
	wlr_log(WLR_DEBUG, "Background dialog destroyed");
}

void
background_dialog_show(struct cg_background_dialog *dialog)
{
	if (!dialog || dialog->is_visible) {
		return;
	}

	/* Get output dimensions */
	struct cg_output *output;
	wl_list_for_each(output, &dialog->server->outputs, link) {
		int width = output->wlr_output->width;
		int height = output->wlr_output->height;

		/* Resize fullscreen background to cover output */
		wlr_scene_rect_set_size(dialog->background, width, height);

		break;  /* Only use first output */
	}

	/* Update results to show all background tabs */
	dialog->query[0] = '\0';
	dialog->query_len = 0;
	background_dialog_update_results(dialog);

	/* Show dialog */
	wlr_scene_node_set_enabled(&dialog->scene_tree->node, true);
	wlr_scene_node_raise_to_top(&dialog->scene_tree->node);
	dialog->is_visible = true;
	dialog->dirty = true;
	background_dialog_update_render(dialog);

	wlr_log(WLR_DEBUG, "Background dialog shown");
}

void
background_dialog_hide(struct cg_background_dialog *dialog)
{
	if (!dialog || !dialog->is_visible) {
		return;
	}

	/* Hide dialog */
	wlr_scene_node_set_enabled(&dialog->scene_tree->node, false);
	dialog->is_visible = false;

	wlr_log(WLR_DEBUG, "Background dialog hidden");
}

void
background_dialog_toggle(struct cg_background_dialog *dialog)
{
	if (!dialog) {
		return;
	}

	if (dialog->is_visible) {
		background_dialog_hide(dialog);
	} else {
		background_dialog_show(dialog);
	}
}

bool
background_dialog_handle_key(struct cg_background_dialog *dialog, xkb_keysym_t sym, uint32_t keycode)
{
	if (!dialog || !dialog->is_visible) {
		return false;
	}

	bool handled = true;

	switch (sym) {
	case XKB_KEY_Escape:
		/* Close dialog */
		background_dialog_hide(dialog);
		break;

	case XKB_KEY_Return:
		/* Select and bring to foreground */
		if (dialog->selected_index < dialog->result_count) {
			struct cg_tab *tab = dialog->results[dialog->selected_index];
			tab_set_background(tab, false);
			tab_activate(tab);
			background_dialog_hide(dialog);
		}
		break;

	case XKB_KEY_Up:
		/* Navigate up */
		if (dialog->selected_index > 0) {
			dialog->selected_index--;
			dialog->dirty = true;
			background_dialog_update_render(dialog);
		}
		break;

	case XKB_KEY_Down:
		/* Navigate down */
		if (dialog->selected_index + 1 < dialog->result_count) {
			dialog->selected_index++;
			dialog->dirty = true;
			background_dialog_update_render(dialog);
		}
		break;

	case XKB_KEY_BackSpace:
		/* Delete last character from query */
		if (dialog->query_len > 0) {
			dialog->query_len--;
			dialog->query[dialog->query_len] = '\0';
			background_dialog_update_results(dialog);
			background_dialog_update_render(dialog);
		} else {
			handled = false;
		}
		break;

	default: {
		/* Regular character - append to query if printable ASCII */
		if (sym >= XKB_KEY_space && sym <= XKB_KEY_asciitilde && sym != XKB_KEY_BackSpace) {
			/* Convert keysym to character */
			char ch = (char)sym;
			if (dialog->query_len < BACKGROUND_DIALOG_MAX_QUERY - 1) {
				dialog->query[dialog->query_len++] = ch;
				dialog->query[dialog->query_len] = '\0';
				background_dialog_update_results(dialog);
				background_dialog_update_render(dialog);
			} else {
				handled = false;
			}
		} else {
			handled = false;
		}
		break;
	}
	}

	return handled;
}
