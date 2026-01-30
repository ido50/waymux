#include "launcher.h"
#include "desktop_entry.h"
#include "output.h"
#include "pixel_buffer.h"
#include "server.h"
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <cairo/cairo.h>
#include <drm/drm_fourcc.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/render/wlr_renderer.h>

/* Semi-transparent dark background (RGBA) */
static const float launcher_bg_color[4] = {
	0.0f,  /* R */
	0.0f,  /* G */
	0.0f,  /* B */
	0.85f  /* A (85% opacity) */
};

/* Launcher UI colors */
static const float launcher_box_bg[4] = {0.12f, 0.12f, 0.12f, 1.0f};  /* Dark box background */
static const float launcher_selected_bg[4] = {0.22f, 0.33f, 0.44f, 1.0f};  /* Selected item */
static const float launcher_text[4] = {1.0f, 1.0f, 1.0f, 1.0f};  /* White text */
static const float launcher_query_bg[4] = {0.08f, 0.08f, 0.08f, 1.0f};  /* Search box */

/* Render launcher UI to a buffer */
static struct wlr_buffer *
render_launcher_ui(struct cg_launcher *launcher, int screen_width, int screen_height)
{
	/* Launcher box dimensions - only render the box, not the whole screen */
	(void)screen_width;
	(void)screen_height;
	int box_width = 600;
	int box_height = 400;

	/* Only allocate buffer for the launcher box area */
	size_t stride = box_width * 4;
	size_t size = box_height * stride;
	uint32_t *data = calloc(1, size);
	if (!data) {
		return NULL;
	}

	/* Create cairo surface */
	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		(unsigned char *)data, CAIRO_FORMAT_ARGB32, box_width, box_height, stride);
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

	/* Draw launcher box background */
	cairo_set_source_rgba(cr, launcher_box_bg[0], launcher_box_bg[1],
			    launcher_box_bg[2], launcher_box_bg[3]);
	cairo_rectangle(cr, 0, 0, box_width, box_height);
	cairo_fill(cr);

	/* Draw search box at top */
	int search_height = 50;
	cairo_set_source_rgba(cr, launcher_query_bg[0], launcher_query_bg[1],
			    launcher_query_bg[2], launcher_query_bg[3]);
	cairo_rectangle(cr, 0, 0, box_width, search_height);
	cairo_fill(cr);

	/* Draw search query text */
	cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
			      CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 14);
	cairo_set_source_rgb(cr, launcher_text[0], launcher_text[1], launcher_text[2]);

	/* Draw query text with cursor */
	char query_display[LAUNCHER_MAX_QUERY + 2];
	snprintf(query_display, sizeof(query_display), "%s|", launcher->query);
	cairo_move_to(cr, 15, 30);
	cairo_show_text(cr, query_display);

	/* Draw results list */
	int results_y = search_height + 10;
	int item_height = 40;
	int max_items = (box_height - search_height - 20) / item_height;

	for (size_t i = 0; i < launcher->result_count && i < (size_t)max_items; i++) {
		int item_y = results_y + i * item_height;

		/* Highlight selected item */
		if (i == launcher->selected_index) {
			cairo_set_source_rgba(cr, launcher_selected_bg[0],
					    launcher_selected_bg[1],
					    launcher_selected_bg[2],
					    launcher_selected_bg[3]);
			cairo_rectangle(cr, 10, item_y, box_width - 20, item_height - 5);
			cairo_fill(cr);
		}

		/* Draw application name */
		struct cg_desktop_entry *entry = launcher->results[i];
		cairo_set_source_rgb(cr, launcher_text[0], launcher_text[1], launcher_text[2]);
		cairo_move_to(cr, 20, item_y + 25);

		/* Truncate name if too long */
		char name_display[100];
		snprintf(name_display, sizeof(name_display), "%s", entry->name);
		cairo_show_text(cr, name_display);
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

	wlr_buffer_init(&buffer->base, &pixel_buffer_impl, box_width, box_height);
	buffer->data = data;
	buffer->width = box_width;
	buffer->height = box_height;
	buffer->size = size;

	return &buffer->base;
}

/* Update the rendered launcher UI */
static void
launcher_update_render(struct cg_launcher *launcher)
{
	if (!launcher->content_buffer || !launcher->is_visible || !launcher->dirty) {
		return;
	}

	/* Get output dimensions and position */
	struct cg_output *output;
	wl_list_for_each(output, &launcher->server->outputs, link) {
		int screen_width = output->wlr_output->width;
		int screen_height = output->wlr_output->height;

		/* Calculate position for centered launcher box */
		int box_width = 600;
		int box_height = 400;
		int box_x = (screen_width - box_width) / 2;
		int box_y = (screen_height - box_height) / 2;

		/* Position the content buffer at the correct location */
		wlr_scene_node_set_position(&launcher->content_buffer->node,
					    box_x, box_y);

		/* Render new UI */
		struct wlr_buffer *new_buffer = render_launcher_ui(launcher,
								screen_width, screen_height);
		if (new_buffer) {
			wlr_scene_buffer_set_buffer(launcher->content_buffer, new_buffer);
			wlr_buffer_drop(new_buffer);
		}

		launcher->dirty = false;

		break; /* Use first output */
	}
}

/*
 * Parse Exec field from desktop entry.
 * Removes XDG format codes (%f, %u, %F, %U, %i, %c, etc.)
 * Returns a newly allocated string that must be freed by caller.
 */
static char *
parse_exec_command(const char *exec)
{
	if (!exec) {
		return NULL;
	}

	char *result = malloc(strlen(exec) + 1);
	if (!result) {
		return NULL;
	}

	size_t j = 0;
	for (size_t i = 0; exec[i] != '\0'; i++) {
		/* Handle format codes: %f, %u, %F, %U, %i, %c, %k, %v */
		if (exec[i] == '%' && exec[i + 1] != '\0') {
			char code = exec[i + 1];
			/* Skip known format codes */
			if (code == 'f' || code == 'u' || code == 'F' || code == 'U' ||
			    code == 'i' || code == 'c' || code == 'k' || code == 'v' ||
			    code == 'd' || code == 'D' || code == 'n' || code == 'N' ||
			    code == 'm' || code == '%') {
				/* Skip the percent sign, and handle double %% */
				if (code != '%') {
					i++; /* Skip the code character too */
					continue;
				}
				/* %% becomes a single % */
			}
		}
		result[j++] = exec[i];
	}
	result[j] = '\0';

	return result;
}

/*
 * Split command string into argv array.
 * Handles simple quoted strings (single and double quotes).
 * Returns NULL on failure.
 */
static char **
parse_argv(const char *command, int *argc_out)
{
	if (!command || !argc_out) {
		return NULL;
	}

	/* Count arguments to allocate array */
	int argc = 0;
	size_t len = strlen(command);
	bool in_quotes = false;
	bool in_single_quotes = false;

	/* First pass: count arguments */
	for (size_t i = 0; i < len; i++) {
		if (command[i] == '\'' && !in_quotes) {
			in_single_quotes = !in_single_quotes;
		} else if (command[i] == '"' && !in_single_quotes) {
			in_quotes = !in_quotes;
		} else if (isspace(command[i]) && !in_quotes && !in_single_quotes) {
			/* Space outside quotes separates arguments */
			if (i > 0 && !isspace(command[i - 1])) {
				argc++;
			}
		}
	}
	/* Count last argument if exists */
	if (len > 0 && !isspace(command[len - 1])) {
		argc++;
	}

	if (argc == 0) {
		return NULL;
	}

	/* Allocate argv array (+1 for NULL terminator) */
	char **argv = calloc(argc + 1, sizeof(char *));
	if (!argv) {
		return NULL;
	}

	/* Second pass: extract arguments */
	int arg_idx = 0;
	size_t arg_start = 0;
	in_quotes = false;
	in_single_quotes = false;

	for (size_t i = 0; i <= len; i++) {
		if (i < len) {
			if (command[i] == '\'' && !in_quotes) {
				in_single_quotes = !in_single_quotes;
				if (arg_idx == 0 || (i > 0 && isspace(command[i - 1]))) {
					arg_start = i + 1;
				}
				continue;
			} else if (command[i] == '"' && !in_single_quotes) {
				in_quotes = !in_quotes;
				if (arg_idx == 0 || (i > 0 && isspace(command[i - 1]))) {
					arg_start = i + 1;
				}
				continue;
			}
		}

		/* End of argument: space outside quotes or end of string */
		if ((i == len || (isspace(command[i]) && !in_quotes && !in_single_quotes)) &&
		    (i > arg_start)) {
			size_t arg_len = i - arg_start;
			argv[arg_idx] = malloc(arg_len + 1);
			if (!argv[arg_idx]) {
				/* Cleanup on failure */
				for (int k = 0; k < arg_idx; k++) {
					free(argv[k]);
				}
				free(argv);
				return NULL;
			}
			memcpy(argv[arg_idx], command + arg_start, arg_len);
			argv[arg_idx][arg_len] = '\0';
			arg_idx++;
			arg_start = i + 1;
		}
	}

	argv[arg_idx] = NULL;
	*argc_out = argc;

	return argv;
}

/* Spawn an application from a desktop entry */
static bool
spawn_application(struct cg_server *server, const char *exec)
{
	if (!exec) {
		wlr_log(WLR_ERROR, "Cannot spawn application: exec is NULL");
		return false;
	}

	/* Parse and clean the exec command */
	char *command = parse_exec_command(exec);
	if (!command) {
		wlr_log(WLR_ERROR, "Failed to parse exec command: %s", exec);
		return false;
	}

	/* Parse command into argv */
	int argc;
	char **argv = parse_argv(command, &argc);
	free(command);

	if (!argv || argc == 0) {
		wlr_log(WLR_ERROR, "Failed to parse argv from command: %s", exec);
		return false;
	}

	/* Log the command we're about to execute */
	wlr_log(WLR_INFO, "Spawning application:");
	for (int i = 0; i < argc; i++) {
		wlr_log(WLR_INFO, "  argv[%d] = %s", i, argv[i]);
	}

	/* Fork and execute */
	pid_t pid = fork();
	if (pid == 0) {
		/* Child process */
		sigset_t set;
		sigemptyset(&set);
		sigprocmask(SIG_SETMASK, &set, NULL);

		/* Reset signal handlers */
		signal(SIGCHLD, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);
		signal(SIGHUP, SIG_DFL);

		/* Execute the application */
		execvp(argv[0], argv);

		/* execvp only returns on failure */
		wlr_log_errno(WLR_ERROR, "Failed to execute %s", argv[0]);
		_exit(1);
	} else if (pid < 0) {
		/* Fork failed */
		wlr_log_errno(WLR_ERROR, "Failed to fork for application launch");
		for (int i = 0; i < argc; i++) {
			free(argv[i]);
		}
		free(argv);
		return false;
	}

	/* Parent process - cleanup argv */
	for (int i = 0; i < argc; i++) {
		free(argv[i]);
	}
	free(argv);

	wlr_log(WLR_INFO, "Application spawned with pid %d", pid);
	return true;
}

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
	launcher->dirty = false;
	launcher->query[0] = '\0';
	launcher->query_len = 0;
	launcher->result_count = 0;
	launcher->selected_index = 0;
	launcher->content_buffer = NULL;

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
	/* Initialize results */
	launcher->result_count = 0;
	launcher->selected_index = 0;

	if (!launcher->server->desktop_entries) {
		return;
	}

	/* Search desktop entries directly into launcher's results array */
	launcher->result_count = desktop_entry_manager_search(
		launcher->server->desktop_entries,
		launcher->query,
		launcher->results,
		256  /* max_results */
	);

	/* Log results for debugging */
	wlr_log(WLR_DEBUG, "Launcher query: '%s', results: %zu",
	        launcher->query, launcher->result_count);
	for (size_t i = 0; i < launcher->result_count; i++) {
		wlr_log(WLR_DEBUG, "  [%zu] %s", i, launcher->results[i]->name);
	}

	/* Mark as dirty for re-rendering */
	launcher->dirty = true;
	launcher_update_render(launcher);
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

	/* Get the first output's dimensions */
	struct cg_output *output;
	wl_list_for_each(output, &launcher->server->outputs, link) {
		struct wlr_output *wlr_output = output->wlr_output;
		int width = wlr_output->width;
		int height = wlr_output->height;

		/* Resize background to match output */
		wlr_scene_rect_set_size(launcher->background, width, height);

		/* Create content buffer for UI if not exists */
		if (!launcher->content_buffer) {
			launcher->content_buffer =
				wlr_scene_buffer_create(launcher->scene_tree, NULL);
		}

		break; /* Use first output for now */
	}

	wlr_scene_node_set_enabled(&launcher->scene_tree->node, true);
	wlr_scene_node_raise_to_top(&launcher->scene_tree->node);
	launcher->is_visible = true;

	/* Update results and render (must be after is_visible is set) */
	launcher_update_results(launcher);

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

			/* Spawn the application */
			if (spawn_application(launcher->server, entry->exec)) {
				wlr_log(WLR_INFO, "Successfully launched: %s", entry->name);
			} else {
				wlr_log(WLR_ERROR, "Failed to launch: %s", entry->name);
			}

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
			launcher->dirty = true;
			launcher_update_render(launcher);
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
			launcher->dirty = true;
			launcher_update_render(launcher);
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
