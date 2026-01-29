#include "launcher.h"
#include "desktop_entry.h"
#include "output.h"
#include "server.h"
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

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
	launcher->query[0] = '\0';
	launcher->query_len = 0;
	launcher->result_count = 0;
	launcher->selected_index = 0;

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

/* Update filtered results based on current query */
static void
launcher_update_results(struct cg_launcher *launcher)
{
	struct wl_list result_list;
	struct cg_desktop_entry *entry, *tmp;

	/* Initialize results */
	launcher->result_count = 0;
	launcher->selected_index = 0;

	if (!launcher->server->desktop_entries) {
		return;
	}

	/* Search desktop entries */
	wl_list_init(&result_list);
	desktop_entry_manager_search(launcher->server->desktop_entries,
	                             launcher->query, &result_list);

	/* Copy results to launcher (up to max) */
	size_t i = 0;
	wl_list_for_each_safe(entry, tmp, &result_list, link) {
		if (i >= 256) {
			break;
		}
		launcher->results[i++] = entry;
		launcher->result_count = i;
	}

	/* Log results for debugging */
	wlr_log(WLR_DEBUG, "Launcher query: '%s', results: %zu",
	        launcher->query, launcher->result_count);
	for (size_t i = 0; i < launcher->result_count; i++) {
		wlr_log(WLR_DEBUG, "  [%zu] %s", i, launcher->results[i]->name);
	}
}

void
launcher_show(struct cg_launcher *launcher)
{
	if (!launcher || launcher->is_visible) {
		return;
	}

	/* Reset query and show all applications */
	launcher->query[0] = '\0';
	launcher->query_len = 0;
	launcher->selected_index = 0;
	launcher_update_results(launcher);

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

/* Handle keyboard input when launcher is visible */
bool
launcher_handle_key(struct cg_launcher *launcher, xkb_keysym_t sym, uint32_t keycode)
{
	if (!launcher || !launcher->is_visible) {
		return false;
	}

	bool handled = true;

	switch (sym) {
	case XKB_KEY_Escape:
		/* Close launcher */
		launcher_hide(launcher);
		break;

	case XKB_KEY_Return:
		/* Launch selected application */
		if (launcher->result_count > 0 &&
		    launcher->selected_index < launcher->result_count) {
			struct cg_desktop_entry *entry =
				launcher->results[launcher->selected_index];
			wlr_log(WLR_INFO, "Launching: %s (%s)",
			        entry->name, entry->exec);
			/* TODO: Actually launch the application in a new tab */
			launcher_hide(launcher);
		}
		break;

	case XKB_KEY_BackSpace:
		/* Remove last character from query */
		if (launcher->query_len > 0) {
			launcher->query_len--;
			launcher->query[launcher->query_len] = '\0';
			launcher_update_results(launcher);
		} else {
			/* If query is empty, close launcher */
			launcher_hide(launcher);
		}
		break;

	case XKB_KEY_Up:
		/* Navigate up in results */
		if (launcher->result_count > 0) {
			if (launcher->selected_index > 0) {
				launcher->selected_index--;
			} else {
				/* Wrap to bottom */
				launcher->selected_index = launcher->result_count - 1;
			}
			wlr_log(WLR_DEBUG, "Selected: %zu/%zu",
			        launcher->selected_index, launcher->result_count);
		}
		break;

	case XKB_KEY_Down:
		/* Navigate down in results */
		if (launcher->result_count > 0) {
			launcher->selected_index++;
			if (launcher->selected_index >= launcher->result_count) {
				launcher->selected_index = 0;  /* Wrap to top */
			}
			wlr_log(WLR_DEBUG, "Selected: %zu/%zu",
			        launcher->selected_index, launcher->result_count);
		}
		break;

	default:
		/* Handle regular character input */
		if (sym >= XKB_KEY_space && sym <= XKB_KEY_asciitilde &&
		    launcher->query_len < LAUNCHER_MAX_QUERY - 1) {
			/* Convert keysym to character */
			char ch = (char)sym;
			launcher->query[launcher->query_len++] = ch;
			launcher->query[launcher->query_len] = '\0';
			launcher_update_results(launcher);
		} else {
			handled = false;
		}
		break;
	}

	return handled;
}
