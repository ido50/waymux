#include "tab_bar.h"

#include "output.h"
#include "server.h"
#include "tab.h"
#include "view.h"
#include "launcher.h"

#include <linux/input-event-codes.h>
#include <wlr/util/log.h>
#include <stdlib.h>

static float
tab_bar_color_active[] = {0.2f, 0.4f, 0.8f, 1.0f};  /* Blue */
static float
tab_bar_color_inactive[] = {0.3f, 0.3f, 0.3f, 1.0f}; /* Gray */
static float
tab_bar_color_bg[] = {0.15f, 0.15f, 0.15f, 1.0f};   /* Dark gray */
static float
tab_bar_color_new_tab[] = {0.3f, 0.6f, 0.3f, 1.0f}; /* Green */

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

	/* Position tab bar at bottom of layout */
	int y = layout_box.height - tab_bar->height;
	wlr_scene_node_set_position(&tab_bar->scene_tree->node, 0, y);

	/* Position tabs */
	int x = TAB_BAR_PADDING;
	for (int i = 0; i < tab_bar->tab_count; i++) {
		struct wlr_scene_node *bg_node =
			&tab_bar->tabs[i].background->node;
		wlr_scene_node_set_position(bg_node, x, TAB_BAR_PADDING);
		x += TAB_BUTTON_WIDTH + TAB_BUTTON_GAP;
	}

	/* Position new tab button on the right */
	int new_tab_x = tab_bar->width - TAB_NEW_TAB_BUTTON_WIDTH - TAB_BAR_PADDING;
	wlr_scene_node_set_position(&tab_bar->new_tab_button.background->node,
		new_tab_x, TAB_BAR_PADDING);
}

void
tab_bar_update(struct cg_tab_bar *tab_bar)
{
	struct cg_server *server = tab_bar->server;

	/* Destroy old tab buttons */
	for (int i = 0; i < tab_bar->tab_count; i++) {
		if (tab_bar->tabs[i].background) {
			wlr_scene_node_destroy(&tab_bar->tabs[i].background->node);
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
			continue;
		}

		index++;
		tab_bar->tab_count++;
	}

	/* Show tab bar if we have tabs */
	if (tab_bar->tab_count > 0) {
		wlr_scene_node_set_enabled(&tab_bar->scene_tree->node, true);
		tab_bar_update_layout(tab_bar);
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
