#ifndef CG_LAUNCHER_H
#define CG_LAUNCHER_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>

struct cg_server;

struct cg_launcher {
	struct cg_server *server;
	struct wlr_scene_tree *scene_tree;
	struct wlr_scene_rect *background;
	bool is_visible;
};

struct cg_launcher *launcher_create(struct cg_server *server);
void launcher_destroy(struct cg_launcher *launcher);
void launcher_show(struct cg_launcher *launcher);
void launcher_hide(struct cg_launcher *launcher);
void launcher_toggle(struct cg_launcher *launcher);

#endif
