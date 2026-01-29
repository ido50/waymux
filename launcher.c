#include "launcher.h"
#include "desktop_entry.h"
#include "output.h"
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

/* Semi-transparent dark background (RGBA) */
static const float launcher_bg_color[4] = {
	0.0f,  /* R */
	0.0f,  /* G */
	0.0f,  /* B */
	0.85f  /* A (85% opacity) */
};

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
