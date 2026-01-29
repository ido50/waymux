#include "launcher.h"
#include "output.h"
#include "server.h"
#include <wlr/util/log.h>

/* Semi-transparent dark background (RGBA) */
static const float launcher_bg_color[4] = {
	0.0f,  /* R */
	0.0f,  /* G */
	0.0f,  /* B */
	0.85f  /* A (85% opacity) */
};

struct cg_launcher *
launcher_create(struct cg_server *server)
{
	struct cg_launcher *launcher = calloc(1, sizeof(struct cg_launcher));
	if (!launcher) {
		wlr_log(WLR_ERROR, "Failed to allocate launcher");
		return NULL;
	}

	launcher->server = server;
	launcher->is_visible = false;

	/* Create scene tree for launcher overlay */
	launcher->scene_tree = wlr_scene_tree_create(&server->scene->tree);
	if (!launcher->scene_tree) {
		wlr_log(WLR_ERROR, "Failed to create launcher scene tree");
		free(launcher);
		return NULL;
	}

	/* Create background rectangle */
	/* Size and position will be set when shown based on output dimensions */
	launcher->background = wlr_scene_rect_create(
		launcher->scene_tree,
		100,  /* Initial width, will be resized */
		100,  /* Initial height, will be resized */
		launcher_bg_color
	);
	if (!launcher->background) {
		wlr_log(WLR_ERROR, "Failed to create launcher background");
		wlr_scene_node_destroy(&launcher->scene_tree->node);
		free(launcher);
		return NULL;
	}

	/* Initially hidden */
	wlr_scene_node_set_enabled(&launcher->scene_tree->node, false);
	wlr_scene_node_raise_to_top(&launcher->scene_tree->node);

	wlr_log(WLR_DEBUG, "Launcher created");
	return launcher;
}

void
launcher_destroy(struct cg_launcher *launcher)
{
	if (!launcher) {
		return;
	}

	/* Scene tree cleanup is handled by wlroots when destroyed */
	wlr_scene_node_destroy(&launcher->scene_tree->node);
	free(launcher);

	wlr_log(WLR_DEBUG, "Launcher destroyed");
}

void
launcher_show(struct cg_launcher *launcher)
{
	if (!launcher || launcher->is_visible) {
		return;
	}

	/* Get the first output's dimensions */
	struct cg_output *output;
	wl_list_for_each(output, &launcher->server->outputs, link) {
		struct wlr_output *wlr_output = output->wlr_output;
		int width = wlr_output->width;
		int height = wlr_output->height;

		/* Resize background to match output */
		wlr_scene_rect_set_size(launcher->background, width, height);

		/* Center the launcher (for now it fills the screen) */
		/* TODO: Make launcher smaller and centered */

		break; /* Use first output for now */
	}

	wlr_scene_node_set_enabled(&launcher->scene_tree->node, true);
	wlr_scene_node_raise_to_top(&launcher->scene_tree->node);
	launcher->is_visible = true;

	wlr_log(WLR_DEBUG, "Launcher shown");
}

void
launcher_hide(struct cg_launcher *launcher)
{
	if (!launcher || !launcher->is_visible) {
		return;
	}

	wlr_scene_node_set_enabled(&launcher->scene_tree->node, false);
	launcher->is_visible = false;

	wlr_log(WLR_DEBUG, "Launcher hidden");
}

void
launcher_toggle(struct cg_launcher *launcher)
{
	if (!launcher) {
		return;
	}

	if (launcher->is_visible) {
		launcher_hide(launcher);
	} else {
		launcher_show(launcher);
	}
}
