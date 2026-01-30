#ifndef CG_BACKGROUND_DIALOG_H
#define CG_BACKGROUND_DIALOG_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include <xkbcommon/xkbcommon.h>

#define BACKGROUND_DIALOG_MAX_QUERY 256

struct cg_server;
struct cg_tab;

struct cg_background_dialog {
	struct cg_server *server;
	struct wlr_scene_tree *scene_tree;
	struct wlr_scene_rect *background;
	struct wlr_scene_buffer *content_buffer;  /* Rendered dialog UI */
	bool is_visible;
	bool dirty;  /* UI needs re-rendering */

	/* Search state */
	char query[BACKGROUND_DIALOG_MAX_QUERY];
	size_t query_len;

	/* Filtered results */
	struct cg_tab *results[256];  /* Simplified: fixed array */
	size_t result_count;
	size_t selected_index;
};

struct cg_background_dialog *background_dialog_create(struct cg_server *server);
void background_dialog_destroy(struct cg_background_dialog *dialog);
void background_dialog_show(struct cg_background_dialog *dialog);
void background_dialog_hide(struct cg_background_dialog *dialog);
void background_dialog_toggle(struct cg_background_dialog *dialog);

/* Keyboard input handling */
bool background_dialog_handle_key(struct cg_background_dialog *dialog, xkb_keysym_t sym, uint32_t keycode);

#endif
