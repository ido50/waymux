#include "tab_bar.h"

#include "output.h"
#include "server.h"
#include "tab.h"
#include "view.h"
#include "launcher.h"

#include <linux/input-event-codes.h>
#include <stdlib.h>
#include <wlr/util/log.h>

static float
tab_bar_color_active[] = {0.2f, 0.4f, 0.8f, 1.0f};  /* Blue */
static float
tab_bar_color_inactive[] = {0.3f, 0.3f, 0.3f, 1.0f}; /* Gray */
static float
tab_bar_color_bg[] = {0.15f, 0.15f, 0.15f, 1.0f};   /* Dark gray */
static float
tab_bar_color_new_tab[] = {0.3f, 0.6f, 0.3f, 1.0f}; /* Green */

#define TAB_TEXT_PADDING 4

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

	/* Create new tab button */
	tab_bar->new_tab_button.background = wlr_scene_rect_create(
		tab_bar->scene_tree, TAB_NEW_TAB_BUTTON_WIDTH,
		TAB_BAR_HEIGHT - 2 * TAB_BAR_PADDING,
		tab_bar_color_new_tab);
	if (!tab_bar->new_tab_button.background) {
		wlr_log(WLR_ERROR, "Failed to create new tab button background");
		tab_bar_destroy(tab_bar);
		return NULL;
	}
	tab_bar->new_tab_button.text_buffer = NULL;
	tab_bar->new_tab_button.text_surface = NULL;

	/* Initialize tab buttons */
	for (int i = 0; i < TAB_BAR_MAX_TABS; i++) {
		tab_bar->tabs[i].background = NULL;
		tab_bar->tabs[i].text_buffer = NULL;
		tab_bar->tabs[i].text_surface = NULL;
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
		if (tab_bar->tabs[i].text_surface) {
			free(tab_bar->tabs[i].text_surface);
		}
		if (tab_bar->tabs[i].text_buffer) {
			wlr_scene_node_destroy(&tab_bar->tabs[i].text_buffer->node);
		}
		if (tab_bar->tabs[i].background) {
			wlr_scene_node_destroy(&tab_bar->tabs[i].background->node);
		}
	}

	if (tab_bar->new_tab_button.text_surface) {
		free(tab_bar->new_tab_button.text_surface);
	}
	if (tab_bar->new_tab_button.text_buffer) {
		wlr_scene_node_destroy(&tab_bar->new_tab_button.text_buffer->node);
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
		if (!tab_bar->tabs[i].background) continue;

		struct wlr_scene_node *bg_node =
			&tab_bar->tabs[i].background->node;
		wlr_scene_node_set_position(bg_node, x, TAB_BAR_PADDING);

		if (tab_bar->tabs[i].text_buffer) {
			wlr_scene_node_set_position(&tab_bar->tabs[i].text_buffer->node,
						    x + TAB_TEXT_PADDING,
						    TAB_BAR_PADDING);
		}

		x += TAB_BUTTON_WIDTH + TAB_BUTTON_GAP;
	}

	/* Position new tab button on the right */
	int new_tab_x = tab_bar->width - TAB_NEW_TAB_BUTTON_WIDTH - TAB_BAR_PADDING;
	wlr_scene_node_set_position(&tab_bar->new_tab_button.background->node,
		new_tab_x, TAB_BAR_PADDING);

	if (tab_bar->new_tab_button.text_buffer) {
		wlr_scene_node_set_position(&tab_bar->new_tab_button.text_buffer->node,
					    new_tab_x + TAB_TEXT_PADDING,
					    TAB_BAR_PADDING);
	}
}

void
tab_bar_update(struct cg_tab_bar *tab_bar)
{
	struct cg_server *server = tab_bar->server;

	/* Clean up old tab buttons */
	for (int i = 0; i < TAB_BAR_MAX_TABS; i++) {
		if (tab_bar->tabs[i].text_surface) {
			free(tab_bar->tabs[i].text_surface);
			tab_bar->tabs[i].text_surface = NULL;
		}
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

		/* Create tab button background */
		tab_bar->tabs[index].background = wlr_scene_rect_create(
			tab_bar->scene_tree, TAB_BUTTON_WIDTH,
			TAB_BAR_HEIGHT - 2 * TAB_BAR_PADDING, color);

		if (!tab_bar->tabs[index].background) {
			wlr_log(WLR_ERROR, "Failed to create tab button background");
			index++;
			tab_bar->tab_count++;
			continue;
		}

		/* Get tab title for display (store as string for future text rendering) */
		char *view_title = NULL;
		if (tab->view) {
			view_title = view_get_title(tab->view);
		}

		/* Store title for future use (text rendering to be implemented) */
		tab_bar->tabs[index].text_surface = view_title;

		index++;
		tab_bar->tab_count++;
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
