#ifndef CG_TAB_BAR_H
#define CG_TAB_BAR_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>

struct cg_server;

/* Maximum number of tabs to display in tab bar */
#define TAB_BAR_MAX_TABS 256

/* Tab bar dimensions */
#define TAB_BAR_HEIGHT 48
#define TAB_BAR_PADDING 6
#define TAB_BUTTON_WIDTH 140
#define TAB_BUTTON_GAP 4
#define TAB_NEW_TAB_BUTTON_WIDTH 80

struct cg_tab_bar_button {
	struct wlr_scene_rect *background;
	struct wlr_scene_buffer *text_buffer;
};

struct cg_tab_bar {
	struct cg_server *server;
	struct wlr_scene_tree *scene_tree;
	struct wlr_scene_rect *background;

	/* Tab buttons */
	struct cg_tab_bar_button tabs[TAB_BAR_MAX_TABS];
	int tab_count;

	/* New Tab button */
	struct cg_tab_bar_button new_tab_button;

	/* Layout */
	int width;
	int height;
};

/* Create and destroy tab bar */
struct cg_tab_bar *tab_bar_create(struct cg_server *server);
void tab_bar_destroy(struct cg_tab_bar *tab_bar);

/* Update tab bar when tabs change */
void tab_bar_update(struct cg_tab_bar *tab_bar);

/* Handle mouse clicks on tab bar */
bool tab_bar_handle_click(struct cg_tab_bar *tab_bar, double x, double y,
	uint32_t button);

#endif
