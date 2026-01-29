#ifndef CG_LAUNCHER_H
#define CG_LAUNCHER_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include <xkbcommon/xkbcommon.h>

#define LAUNCHER_MAX_QUERY 256

struct cg_server;
struct cg_desktop_entry;

struct cg_launcher {
	struct cg_server *server;
	struct wlr_scene_tree *scene_tree;
	struct wlr_scene_rect *background;
	bool is_visible;

	/* Search state */
	char query[LAUNCHER_MAX_QUERY];
	size_t query_len;

	/* Filtered results */
	struct cg_desktop_entry *results[256];  /* Simplified: fixed array */
	size_t result_count;
	size_t selected_index;
};

struct cg_launcher *launcher_create(struct cg_server *server);
void launcher_destroy(struct cg_launcher *launcher);
void launcher_show(struct cg_launcher *launcher);
void launcher_hide(struct cg_launcher *launcher);
void launcher_toggle(struct cg_launcher *launcher);

/* Keyboard input handling */
bool launcher_handle_key(struct cg_launcher *launcher, xkb_keysym_t sym, uint32_t keycode);

#endif
