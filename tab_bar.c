#include "tab_bar.h"

#include "output.h"
#include "server.h"
#include "tab.h"
#include "view.h"
#include "launcher.h"

#include <linux/input-event-codes.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/util/log.h>
#include <cairo/cairo.h>
#include <drm/drm_fourcc.h>
#include <wlr/interfaces/wlr_buffer.h>

static float
tab_bar_color_active[] = {0.22f, 0.33f, 0.44f, 1.0f};  /* Kitty-like active blue-gray */
static float
tab_bar_color_inactive[] = {0.12f, 0.12f, 0.12f, 1.0f}; /* Dark gray for inactive */
static float
tab_bar_color_bg[] = {0.0f, 0.0f, 0.0f, 1.0f};       /* Pure black background */
static float
tab_bar_color_new_tab[] = {0.16f, 0.16f, 0.16f, 1.0f};  /* Slightly lighter than inactive */

#define TAB_TEXT_PADDING 4

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

/* Create a wlr_buffer with rendered text and background */
static struct wlr_buffer *
create_text_buffer(const char *text, int width, int height, float *bg_color)
{
	/* Allocate buffer data */
	size_t stride = width * 4;
	size_t size = height * stride;
	uint32_t *data = calloc(1, size);
	if (!data) {
		return NULL;
	}

	/* Create cairo surface */
	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		(unsigned char *)data, CAIRO_FORMAT_ARGB32, width, height, stride);
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

	/* Draw background */
	cairo_set_source_rgba(cr, bg_color[0], bg_color[1], bg_color[2], bg_color[3]);
	cairo_paint(cr);

	/* Draw text */
	if (text && strlen(text) > 0) {
		cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
				      CAIRO_FONT_WEIGHT_NORMAL);
		cairo_set_font_size(cr, 11);
		cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); /* White text */

		/* Truncate text if too long */
		char truncated[256];
		snprintf(truncated, sizeof(truncated), "%s", text);

		/* Center text horizontally and vertically */
		cairo_text_extents_t extents;
		cairo_text_extents(cr, truncated, &extents);

		double x = (width - extents.width) / 2 - extents.x_bearing;
		double y = (height + extents.height) / 2 - extents.y_bearing;

		cairo_move_to(cr, x, y);
		cairo_show_text(cr, truncated);
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

	wlr_buffer_init(&buffer->base, &pixel_buffer_impl, width, height);
	buffer->data = data;
	buffer->width = width;
	buffer->height = height;
	buffer->size = size;

	return &buffer->base;
}

struct cg_tab_bar *
tab_bar_create(struct cg_server *server)
{
	struct cg_tab_bar *tab_bar = calloc(1, sizeof(struct cg_tab_bar));
	if (!tab_bar) {
		wlr_log(WLR_ERROR, "Failed to allocate tab bar");
		return NULL;
	}

	tab_bar->server = server;
	tab_bar->height = TAB_BAR_HEIGHT;

	/* Create scene tree for tab bar */
	tab_bar->scene_tree = wlr_scene_tree_create(&server->scene->tree);
	if (!tab_bar->scene_tree) {
		wlr_log(WLR_ERROR, "Failed to create tab bar scene tree");
		free(tab_bar);
		return NULL;
	}

	/* Create background */
	tab_bar->background = wlr_scene_rect_create(tab_bar->scene_tree,
		1, 1, tab_bar_color_bg);
	if (!tab_bar->background) {
		wlr_log(WLR_ERROR, "Failed to create tab bar background");
		wlr_scene_node_destroy(&tab_bar->scene_tree->node);
		free(tab_bar);
		return NULL;
	}

	/* Create new tab button (background + text will be in buffer) */
	tab_bar->new_tab_button.background = NULL;
	tab_bar->new_tab_button.text_buffer = NULL;

	/* Initialize tab buttons */
	for (int i = 0; i < TAB_BAR_MAX_TABS; i++) {
		tab_bar->tabs[i].background = NULL;
		tab_bar->tabs[i].text_buffer = NULL;
	}

	/* Initially hide tab bar until we have tabs */
	wlr_scene_node_set_enabled(&tab_bar->scene_tree->node, false);

	wlr_log(WLR_DEBUG, "Created tab bar");
	return tab_bar;
}

void
tab_bar_destroy(struct cg_tab_bar *tab_bar)
{
	if (!tab_bar) {
		return;
	}

	/* Clean up old tab buttons */
	for (int i = 0; i < TAB_BAR_MAX_TABS; i++) {
		if (tab_bar->tabs[i].text_buffer) {
			wlr_scene_node_destroy(&tab_bar->tabs[i].text_buffer->node);
			tab_bar->tabs[i].text_buffer = NULL;
		}
	}

	if (tab_bar->new_tab_button.text_buffer) {
		wlr_scene_node_destroy(&tab_bar->new_tab_button.text_buffer->node);
		tab_bar->new_tab_button.text_buffer = NULL;
	}

	/* Scene tree cleanup destroys all children */
	if (tab_bar->scene_tree) {
		wlr_scene_node_destroy(&tab_bar->scene_tree->node);
	}

	free(tab_bar);
	wlr_log(WLR_DEBUG, "Destroyed tab bar");
}

static void
tab_bar_update_layout(struct cg_tab_bar *tab_bar)
{
	struct cg_server *server = tab_bar->server;
	struct wlr_box layout_box;

	/* Get the overall output layout dimensions */
	wlr_output_layout_get_box(server->output_layout, NULL, &layout_box);

	/* Update dimensions */
	tab_bar->width = layout_box.width;
	wlr_scene_rect_set_size(tab_bar->background,
		tab_bar->width, tab_bar->height);

	/* Position tab bar at top of layout */
	wlr_scene_node_set_position(&tab_bar->scene_tree->node, 0, 0);

	wlr_log(WLR_DEBUG, "Tab bar layout: width=%d, height=%d, y=0 (top), layout_height=%d",
		tab_bar->width, tab_bar->height, layout_box.height);

	/* Position tabs */
	int x = TAB_BAR_PADDING;
	for (int i = 0; i < tab_bar->tab_count; i++) {
		if (tab_bar->tabs[i].text_buffer) {
			wlr_scene_node_set_position(&tab_bar->tabs[i].text_buffer->node,
						    x, TAB_BAR_PADDING);
		}

		x += TAB_BUTTON_WIDTH + TAB_BUTTON_GAP;
	}

	/* Position new tab button on the right */
	int new_tab_x = tab_bar->width - TAB_NEW_TAB_BUTTON_WIDTH - TAB_BAR_PADDING;
	if (tab_bar->new_tab_button.text_buffer) {
		wlr_scene_node_set_position(&tab_bar->new_tab_button.text_buffer->node,
					    new_tab_x, TAB_BAR_PADDING);
	}
}

void
tab_bar_update(struct cg_tab_bar *tab_bar)
{
	struct cg_server *server = tab_bar->server;

	/* Clean up old tab buttons */
	for (int i = 0; i < TAB_BAR_MAX_TABS; i++) {
		if (tab_bar->tabs[i].text_buffer) {
			wlr_scene_node_destroy(&tab_bar->tabs[i].text_buffer->node);
			tab_bar->tabs[i].text_buffer = NULL;
		}
		if (tab_bar->tabs[i].background) {
			wlr_scene_node_destroy(&tab_bar->tabs[i].background->node);
			tab_bar->tabs[i].background = NULL;
		}
	}

	tab_bar->tab_count = 0;

	/* Create new tab buttons */
	struct cg_tab *tab;
	int index = 0;

	wl_list_for_each(tab, &server->tabs, link) {
		if (index >= TAB_BAR_MAX_TABS) {
			wlr_log(WLR_ERROR, "Too many tabs for tab bar");
			break;
		}

		/* Determine if this is the active tab */
		bool is_active = (tab == server->active_tab);
		float *color = is_active ? tab_bar_color_active :
			tab_bar_color_inactive;

		/* Get tab title and app_id for display */
		char *view_title = NULL;
		char *view_app_id = NULL;
		if (tab->view) {
			view_title = view_get_title(tab->view);
			view_app_id = view_get_app_id(tab->view);
		}

		/* Create display text: app_id followed by title (truncated if needed) */
		char display_text[512];
		if (view_app_id && view_title) {
			snprintf(display_text, sizeof(display_text), "%s: %s",
				 view_app_id, view_title);
		} else if (view_title) {
			snprintf(display_text, sizeof(display_text), "%s", view_title);
		} else if (view_app_id) {
			snprintf(display_text, sizeof(display_text), "%s", view_app_id);
		} else {
			snprintf(display_text, sizeof(display_text), "Tab %d", index + 1);
		}

		/* Create buffer with rendered text and background */
		struct wlr_buffer *buffer = create_text_buffer(
			display_text, TAB_BUTTON_WIDTH,
			TAB_BAR_HEIGHT - 2 * TAB_BAR_PADDING, color);

		if (buffer) {
			tab_bar->tabs[index].text_buffer =
				wlr_scene_buffer_create(tab_bar->scene_tree, buffer);
			wlr_buffer_drop(buffer); /* scene_buffer holds reference */
		}

		free(view_title);
		free(view_app_id);

		index++;
		tab_bar->tab_count++;
	}

	/* Create text buffer for new tab button */
	struct wlr_buffer *new_tab_buffer = create_text_buffer(
		"+", TAB_NEW_TAB_BUTTON_WIDTH,
		TAB_BAR_HEIGHT - 2 * TAB_BAR_PADDING, tab_bar_color_new_tab);

	if (new_tab_buffer) {
		tab_bar->new_tab_button.text_buffer =
			wlr_scene_buffer_create(tab_bar->scene_tree, new_tab_buffer);
		wlr_buffer_drop(new_tab_buffer);
	}

	/* Show tab bar if we have tabs */
	if (tab_bar->tab_count > 0) {
		wlr_scene_node_set_enabled(&tab_bar->scene_tree->node, true);
		tab_bar_update_layout(tab_bar);

		/* Raise tab bar to top so it appears above views */
		wlr_scene_node_raise_to_top(&tab_bar->scene_tree->node);

		/* Reposition all views to account for tab bar space */
		view_position_all(server);
	} else {
		wlr_scene_node_set_enabled(&tab_bar->scene_tree->node, false);
	}
}

bool
tab_bar_handle_click(struct cg_tab_bar *tab_bar, double x, double y,
	uint32_t button)
{
	if (!tab_bar->scene_tree->node.enabled) {
		return false;
	}

	/* Check if click is within tab bar vertical bounds */
	if (y < 0 || y >= tab_bar->height) {
		return false;
	}

	/* Check if click is on a tab button */
	int tab_x = TAB_BAR_PADDING;
	for (int i = 0; i < tab_bar->tab_count; i++) {
		if (x >= tab_x && x < tab_x + TAB_BUTTON_WIDTH) {
			struct cg_tab *tab = NULL;
			int index = 0;
			struct cg_tab *t;

			/* Find the clicked tab */
			wl_list_for_each(t, &tab_bar->server->tabs, link) {
				if (index == i) {
					tab = t;
					break;
				}
				index++;
			}

			if (tab) {
				if (button == BTN_LEFT) {
					/* Left click: activate tab */
					tab_activate(tab);
					return true;
				} else if (button == BTN_MIDDLE) {
					/* Middle click: close tab */
					tab_destroy(tab);
					return true;
				}
			}
			return false;
		}
		tab_x += TAB_BUTTON_WIDTH + TAB_BUTTON_GAP;
	}

	/* Check if click is on new tab button */
	int new_tab_x = tab_bar->width - TAB_NEW_TAB_BUTTON_WIDTH -
		TAB_BAR_PADDING;
	if (x >= new_tab_x && x < new_tab_x + TAB_NEW_TAB_BUTTON_WIDTH) {
		if (button == BTN_LEFT) {
			/* Show launcher */
			launcher_show(tab_bar->server->launcher);
			return true;
		}
	}

	return false;
}
