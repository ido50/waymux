#include "tab_bar.h"

#include "output.h"
#include "pixel_buffer.h"
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
#include <math.h>

/* M_PI might not be defined on all systems */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2 (M_PI / 2.0)
#endif

/* Modern browser-inspired color scheme */
static float
tab_bar_color_bg[] = {0.11f, 0.11f, 0.12f, 1.0f};  /* Dark toolbar background */
static float
tab_bar_color_active[] = {0.26f, 0.28f, 0.32f, 1.0f};  /* Lighter gray for active tab */
static float
tab_bar_color_inactive[] = {0.18f, 0.19f, 0.21f, 1.0f};  /* Darker gray for inactive */
static float
tab_bar_color_border[] = {0.0f, 0.0f, 0.0f, 1.0f};  /* Black borders */
static float
tab_bar_color_text_inactive[] = {0.7f, 0.7f, 0.7f, 1.0f};  /* Gray text for inactive */
static float
tab_bar_color_text_active[] = {1.0f, 1.0f, 1.0f, 1.0f};  /* White text for active */
static float
tab_bar_color_new_tab_bg[] = {0.15f, 0.16f, 0.18f, 1.0f};  /* New tab button */

#define TAB_TEXT_PADDING_SIDES 16
#define TAB_TEXT_TOP_OFFSET 10
#define TAB_FONT_SIZE 11
#define TAB_ELLIPSIS "..."
#define TAB_CLOSE_BUTTON_SIZE 16
#define TAB_CLOSE_BUTTON_PADDING 4

/* Draw a rounded rectangle path */
static void
draw_rounded_rect(cairo_t *cr, double x, double y, double width, double height,
		  double radius, bool top_only)
{
	cairo_new_sub_path(cr);
	if (top_only) {
		/* Rounded top corners, square bottom corners (browser tab style) */
		cairo_move_to(cr, x, y + height);
		cairo_line_to(cr, x, y + radius);
		cairo_arc(cr, x + radius, y + radius, radius, M_PI, -M_PI_2);
		cairo_line_to(cr, x + width - radius, y);
		cairo_arc(cr, x + width - radius, y + radius, radius, -M_PI_2, 0);
		cairo_line_to(cr, x + width, y + height);
	} else {
		/* All corners rounded */
		cairo_arc(cr, x + radius, y + radius, radius, M_PI, -M_PI_2);
		cairo_arc(cr, x + width - radius, y + radius, radius, -M_PI_2, 0);
		cairo_arc(cr, x + width - radius, y + height - radius, radius, 0, M_PI_2);
		cairo_arc(cr, x + radius, y + height - radius, radius, M_PI_2, M_PI);
	}
	cairo_close_path(cr);
}

/* Calculate text width with proper font metrics */
static double
get_text_width(cairo_t *cr, const char *text)
{
	cairo_text_extents_t extents;
	cairo_text_extents(cr, text, &extents);
	return extents.width;
}

/* Truncate text to fit width with ellipsis */
static void
truncate_text_to_width(char *dest, size_t dest_size, cairo_t *cr,
		       const char *text, double max_width)
{
	if (get_text_width(cr, text) <= max_width) {
		snprintf(dest, dest_size, "%s", text);
		return;
	}

	/* Binary search for best truncation point */
	size_t len = strlen(text);
	size_t left = 0;
	size_t right = len;
	size_t best = 0;

	char buf[512];
	const char *ellipsis = TAB_ELLIPSIS;

	while (left < right) {
		size_t mid = (left + right + 1) / 2;
		snprintf(buf, sizeof(buf), "%.*s%s", (int)mid, text, ellipsis);

		if (get_text_width(cr, buf) <= max_width) {
			best = mid;
			left = mid;
		} else {
			right = mid - 1;
		}
	}

	if (best > 0) {
		snprintf(dest, dest_size, "%.*s%s", (int)best, text, ellipsis);
	} else {
		/* Even with ellipsis only, it's too long - truncate ellipsis itself */
		snprintf(dest, dest_size, "..");
	}
}

/* Calculate tab width based on text content */
static int
calculate_tab_width(cairo_t *cr, const char *text)
{
	double text_width = get_text_width(cr, text);
	int width = (int)ceil(text_width + TAB_TEXT_PADDING_SIDES * 2);

	if (width < TAB_BUTTON_MIN_WIDTH) {
		return TAB_BUTTON_MIN_WIDTH;
	}
	if (width > TAB_BUTTON_MAX_WIDTH) {
		return TAB_BUTTON_MAX_WIDTH;
	}
	return width;
}

/* Create a wlr_buffer with rendered browser-style tab */
static struct wlr_buffer *
create_tab_buffer(const char *text, int width, int height, bool is_active,
		  bool show_close)
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

	/* Clear background */
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_paint(cr);

	/* Draw tab background with rounded top corners */
	float *bg_color = is_active ? tab_bar_color_active : tab_bar_color_inactive;
	cairo_set_source_rgba(cr, bg_color[0], bg_color[1], bg_color[2], bg_color[3]);

	/* Inset by 1px on each side for border */
	draw_rounded_rect(cr, 0.5, 0.5, width - 1, height - 0.5, TAB_CORNER_RADIUS, true);
	cairo_fill(cr);

	/* Draw border */
	cairo_set_source_rgba(cr, tab_bar_color_border[0],
			     tab_bar_color_border[1],
			     tab_bar_color_border[2],
			     tab_bar_color_border[3]);
	cairo_set_line_width(cr, 1);
	draw_rounded_rect(cr, 0.5, 0.5, width - 1, height - 0.5, TAB_CORNER_RADIUS, true);
	cairo_stroke(cr);

	/* Draw text */
	if (text && strlen(text) > 0) {
		cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
				      CAIRO_FONT_WEIGHT_NORMAL);
		cairo_set_font_size(cr, TAB_FONT_SIZE);

		float *text_color = is_active ? tab_bar_color_text_active :
					tab_bar_color_text_inactive;
		cairo_set_source_rgba(cr, text_color[0], text_color[1],
				    text_color[2], text_color[3]);

		/* Truncate text to fit available space */
		char truncated[256];
		double available_width = width - TAB_TEXT_PADDING_SIDES * 2;
		if (show_close) {
			available_width -= TAB_CLOSE_BUTTON_SIZE + TAB_CLOSE_BUTTON_PADDING;
		}
		truncate_text_to_width(truncated, sizeof(truncated), cr, text, available_width);

		/* Position text */
		cairo_text_extents_t extents;
		cairo_text_extents(cr, truncated, &extents);

		double x = TAB_TEXT_PADDING_SIDES - extents.x_bearing;
		double y = TAB_TEXT_TOP_OFFSET - extents.y_bearing;

		cairo_move_to(cr, x, y);
		cairo_show_text(cr, truncated);
	}

	/* Draw close button (X) */
	if (show_close) {
		double close_x = width - TAB_CLOSE_BUTTON_SIZE - TAB_CLOSE_BUTTON_PADDING;
		double close_y = (height - TAB_CLOSE_BUTTON_SIZE) / 2.0;

		/* Draw subtle background for close button on hover area */
		cairo_set_source_rgba(cr, 1, 1, 1, 0.1);
		draw_rounded_rect(cr, close_x, close_y, TAB_CLOSE_BUTTON_SIZE,
				 TAB_CLOSE_BUTTON_SIZE, 3, false);
		cairo_fill(cr);

		/* Draw X */
		cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, 1.0);
		cairo_set_line_width(cr, 1.5);

		double center_x = close_x + TAB_CLOSE_BUTTON_SIZE / 2.0;
		double center_y = close_y + TAB_CLOSE_BUTTON_SIZE / 2.0;
		double offset = TAB_CLOSE_BUTTON_SIZE / 4.0;

		cairo_move_to(cr, center_x - offset, center_y - offset);
		cairo_line_to(cr, center_x + offset, center_y + offset);
		cairo_move_to(cr, center_x + offset, center_y - offset);
		cairo_line_to(cr, center_x - offset, center_y + offset);
		cairo_stroke(cr);
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

/* Create a wlr_buffer with new tab button (+) */
static struct wlr_buffer *
create_new_tab_buffer(int width, int height)
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

	/* Clear background */
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_paint(cr);

	/* Draw rounded background */
	cairo_set_source_rgba(cr, tab_bar_color_new_tab_bg[0],
			     tab_bar_color_new_tab_bg[1],
			     tab_bar_color_new_tab_bg[2],
			     tab_bar_color_new_tab_bg[3]);
	draw_rounded_rect(cr, 0.5, 0.5, width - 1, height - 0.5, TAB_CORNER_RADIUS, true);
	cairo_fill(cr);

	/* Draw border */
	cairo_set_source_rgba(cr, tab_bar_color_border[0],
			     tab_bar_color_border[1],
			     tab_bar_color_border[2],
			     tab_bar_color_border[3]);
	cairo_set_line_width(cr, 1);
	draw_rounded_rect(cr, 0.5, 0.5, width - 1, height - 0.5, TAB_CORNER_RADIUS, true);
	cairo_stroke(cr);

	/* Draw + icon */
	cairo_set_source_rgba(cr, 0.7, 0.7, 0.7, 1.0);
	cairo_set_line_width(cr, 1.5);

	double center_x = width / 2.0;
	double center_y = height / 2.0;
	double icon_size = 8;

	cairo_move_to(cr, center_x - icon_size / 2, center_y);
	cairo_line_to(cr, center_x + icon_size / 2, center_y);
	cairo_move_to(cr, center_x, center_y - icon_size / 2);
	cairo_line_to(cr, center_x, center_y + icon_size / 2);
	cairo_stroke(cr);

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
		tab_bar->tabs[i].width = TAB_BUTTON_MIN_WIDTH;
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
						    x, 0);
		}

		x += tab_bar->tabs[i].width + TAB_BUTTON_GAP;
	}

	/* Position new tab button on the right */
	int new_tab_x = tab_bar->width - TAB_NEW_TAB_BUTTON_WIDTH - TAB_BAR_PADDING;
	if (tab_bar->new_tab_button.text_buffer) {
		wlr_scene_node_set_position(&tab_bar->new_tab_button.text_buffer->node,
					    new_tab_x, 0);
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

	/* Create a temporary cairo context for text measurement */
	cairo_surface_t *dummy_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
	cairo_t *cr = cairo_create(dummy_surface);
	cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
			      CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, TAB_FONT_SIZE);

	/* Create new tab buttons */
	struct cg_tab *tab;
	int index = 0;

	wl_list_for_each(tab, &server->tabs, link) {
		if (index >= TAB_BAR_MAX_TABS) {
			wlr_log(WLR_ERROR, "Too many tabs for tab bar");
			break;
		}

		/* Skip tabs without views (being destroyed) or background tabs */
		if (!tab->view || tab->is_background) {
			continue;
		}

		/* Determine if this is the active tab */
		bool is_active = (tab == server->active_tab);

		/* Get tab title and app_id for display */
		char *view_title = NULL;
		char *view_app_id = NULL;
		if (tab->view) {
			view_title = view_get_title(tab->view);
			view_app_id = view_get_app_id(tab->view);
		}

		/* Create display text: app_id followed by title */
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

		/* Calculate tab width based on text content */
		int tab_width = calculate_tab_width(cr, display_text);
		tab_bar->tabs[index].width = tab_width;

		/* Create buffer with rendered tab */
		struct wlr_buffer *buffer = create_tab_buffer(
			display_text, tab_width, TAB_BAR_HEIGHT, is_active,
			true);  /* Show close button */

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

	cairo_destroy(cr);
	cairo_surface_destroy(dummy_surface);

	/* Create text buffer for new tab button */
	struct wlr_buffer *new_tab_buffer = create_new_tab_buffer(
		TAB_NEW_TAB_BUTTON_WIDTH, TAB_BAR_HEIGHT);

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
		int tab_width = tab_bar->tabs[i].width;
		if (x >= tab_x && x < tab_x + tab_width) {
			struct cg_tab *tab = NULL;
			int index = 0;
			struct cg_tab *t;

			/* Find the clicked tab */
			wl_list_for_each(t, &tab_bar->server->tabs, link) {
				if (!t->view) continue;  /* Skip tabs being destroyed */
				if (index == i) {
					tab = t;
					break;
				}
				index++;
			}

			if (tab) {
				/* Check if click is on close button */
				int close_x = tab_x + tab_width - TAB_CLOSE_BUTTON_SIZE -
					     TAB_CLOSE_BUTTON_PADDING;
				bool on_close_button = (x >= close_x && x < tab_x + tab_width);

				if (on_close_button && button == BTN_LEFT) {
					/* Click on close button */
					tab_destroy(tab);
					return true;
				} else if (button == BTN_LEFT) {
					/* Left click on tab: activate */
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
		tab_x += tab_width + TAB_BUTTON_GAP;
	}

	/* Check if click is on new tab button */
	int new_tab_x = tab_bar->width - TAB_NEW_TAB_BUTTON_WIDTH - TAB_BAR_PADDING;
	if (x >= new_tab_x && x < new_tab_x + TAB_NEW_TAB_BUTTON_WIDTH) {
		if (button == BTN_LEFT) {
			/* Show launcher */
			launcher_show(tab_bar->server->launcher);
			return true;
		}
	}

	return false;
}
