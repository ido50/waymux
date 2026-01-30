/*
 * Control server for WayMux
 *
 * Accepts commands over Unix domain socket to control tabs and launcher
 */

#include "control.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/util/log.h>

#include "launcher.h"
#include "tab.h"
#include "view.h"

#define CONTROL_BUFFER_SIZE 4096

struct cg_control_client {
	struct cg_control_server *control;
	int fd;
	struct wl_event_source *event_source;
	struct wl_list link; // control_server::clients
	char buffer[CONTROL_BUFFER_SIZE];
	size_t buffer_len;
};

/* Forward declarations */
static void control_client_destroy(struct cg_control_client *client);
static int handle_client_data(int fd, uint32_t mask, void *data);

static void
set_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL);
	if (flags == -1) {
		wlr_log_errno(WLR_ERROR, "Unable to get socket flags");
		return;
	}
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		wlr_log_errno(WLR_ERROR, "Unable to set socket non-blocking");
	}
}

static void
control_client_send(struct cg_control_client *client, const char *message)
{
	size_t len = strlen(message);
	ssize_t sent = send(client->fd, message, len, MSG_NOSIGNAL);
	if (sent < 0) {
		wlr_log_errno(WLR_ERROR, "Failed to send response to client");
		return;
	}

	/* Shutdown write side to signal response is complete */
	shutdown(client->fd, SHUT_WR);
}

static void
handle_list_tabs(struct cg_control_client *client)
{
	struct cg_server *server = client->control->server;
	int index = 0;
	struct cg_tab *tab;

	char response[CONTROL_BUFFER_SIZE];
	size_t offset = 0;

	offset += snprintf(response + offset, CONTROL_BUFFER_SIZE - offset, "OK %d\n", tab_count(server));

	wl_list_for_each(tab, &server->tabs, link) {
		const char *title = "(unnamed)";
		const char *app_id = "(unknown)";
		char *view_title = NULL;
		char *view_app_id = NULL;

		if (tab->view) {
			view_title = view_get_title(tab->view);
			view_app_id = view_get_app_id(tab->view);
			if (view_title) {
				title = view_title;
			}
			if (view_app_id) {
				app_id = view_app_id;
			}
		}

		offset += snprintf(response + offset, CONTROL_BUFFER_SIZE - offset,
				  "%d: [%s] %s\n", index, app_id, title);
		index++;
		free(view_title);
		free(view_app_id);
	}

	control_client_send(client, response);
}

static void
handle_focus_tab(struct cg_control_client *client, const char *arg)
{
	if (!arg || strlen(arg) == 0) {
		control_client_send(client, "ERROR Missing tab index\n");
		return;
	}

	char *endptr;
	long tab_num = strtol(arg, &endptr, 10);
	if (*endptr != '\0' || tab_num < 0) {
		control_client_send(client, "ERROR Invalid tab index\n");
		return;
	}

	struct cg_server *server = client->control->server;
	int index = 0;
	struct cg_tab *tab;

	wl_list_for_each(tab, &server->tabs, link) {
		if (index == tab_num) {
			tab_activate(tab);
			control_client_send(client, "OK\n");
			return;
		}
		index++;
	}

	control_client_send(client, "ERROR Tab index out of range\n");
}

static void
handle_close_tab(struct cg_control_client *client, const char *arg, bool force)
{
	if (!arg || strlen(arg) == 0) {
		control_client_send(client, "ERROR Missing tab index\n");
		return;
	}

	char *endptr;
	long tab_num = strtol(arg, &endptr, 10);
	if (*endptr != '\0' || tab_num < 0) {
		control_client_send(client, "ERROR Invalid tab index\n");
		return;
	}

	struct cg_server *server = client->control->server;
	int index = 0;
	struct cg_tab *tab;

	wl_list_for_each(tab, &server->tabs, link) {
		if (index == tab_num) {
			if (force) {
				/* Kill the view */
				if (tab->view && tab->view->impl && tab->view->impl->close) {
					tab->view->impl->close(tab->view);
				}
			} else {
				/* Graceful close */
				tab_destroy(tab);
			}
			control_client_send(client, "OK\n");
			return;
		}
		index++;
	}

	control_client_send(client, "ERROR Tab index out of range\n");
}

static void
handle_show_launcher(struct cg_control_client *client)
{
	struct cg_server *server = client->control->server;
	launcher_show(server->launcher);
	control_client_send(client, "OK\n");
}

static void
handle_new_tab(struct cg_control_client *client, const char *cmd)
{
	if (!cmd || strlen(cmd) == 0) {
		control_client_send(client, "ERROR Missing command\n");
		return;
	}

	/* Parse command into argv */
	char *cmd_copy = strdup(cmd);
	if (!cmd_copy) {
		control_client_send(client, "ERROR Out of memory\n");
		return;
	}

	/* Count arguments */
	int argc = 0;
	char *saveptr = NULL;
	char *token = strtok_r(cmd_copy, " ", &saveptr);
	while (token) {
		argc++;
		token = strtok_r(NULL, " ", &saveptr);
	}

	if (argc == 0) {
		free(cmd_copy);
		control_client_send(client, "ERROR Empty command\n");
		return;
	}

	/* Allocate argv array */
	char **argv = calloc(argc + 1, sizeof(char *));
	if (!argv) {
		free(cmd_copy);
		control_client_send(client, "ERROR Out of memory\n");
		return;
	}

	/* Parse arguments again */
	strcpy(cmd_copy, cmd);
	token = strtok_r(cmd_copy, " ", &saveptr);
	int i = 0;
	while (token && i < argc) {
		argv[i++] = token;
		token = strtok_r(NULL, " ", &saveptr);
	}
	argv[argc] = NULL;

	/* Fork and exec the command */
	pid_t pid = fork();
	if (pid == 0) {
		/* Child process */
		sigset_t set;
		sigemptyset(&set);
		sigprocmask(SIG_SETMASK, &set, NULL);

		/* Clear WAYLAND_SOCKET to prevent inheriting parent's connection */
		unsetenv("WAYLAND_SOCKET");

		/* Clear DISPLAY to prevent direct X11 connection */
		unsetenv("DISPLAY");

		/* Set WAYLAND_DISPLAY to point to this WayMux instance */
		const char *socket = client->control->server->wl_display_socket;
		if (socket) {
			wlr_log(WLR_DEBUG, "Setting WAYLAND_DISPLAY=%s for new tab", socket);
			setenv("WAYLAND_DISPLAY", socket, 1);

			/* Verify it was set */
			const char *verify = getenv("WAYLAND_DISPLAY");
			wlr_log(WLR_DEBUG, "WAYLAND_DISPLAY is now: %s", verify ? verify : "(NULL)");
		} else {
			wlr_log(WLR_ERROR, "WayMux socket name is NULL! Using parent display.");
		}

		wlr_log(WLR_DEBUG, "Executing: %s", argv[0]);

		/* Special handling for Firefox to prevent single-instance behavior */
		if (strcmp(argv[0], "firefox") == 0 || strcmp(argv[0], "firefox-bin") == 0) {
			/* Check if --new-instance is already in args */
			bool has_new_instance = false;
			for (int i = 1; argv[i] != NULL; i++) {
				if (strcmp(argv[i], "--new-instance") == 0) {
					has_new_instance = true;
					break;
				}
			}

			if (!has_new_instance) {
				wlr_log(WLR_DEBUG, "Adding --new-instance flag for Firefox");
				/* Create new argv with --new-instance inserted */
				char **new_argv = calloc(argc + 2, sizeof(char *));
				if (new_argv) {
					new_argv[0] = argv[0];
					new_argv[1] = "--new-instance";
					for (int i = 1; argv[i] != NULL; i++) {
						new_argv[i + 1] = argv[i];
					}
					execvp(argv[0], new_argv);
					free(new_argv);
				}
			}
		}

		execvp(argv[0], argv);
		/* execvp only returns on failure */
		wlr_log_errno(WLR_ERROR, "execvp failed");
		_exit(1);
	} else if (pid < 0) {
		free(cmd_copy);
		free(argv);
		control_client_send(client, "ERROR Failed to fork\n");
		return;
	}

	free(cmd_copy);
	free(argv);

	wlr_log(WLR_DEBUG, "Started new tab process with pid %d", pid);
	control_client_send(client, "OK\n");
}

static void
process_command(struct cg_control_client *client, const char *command)
{
	/* Parse command */
	if (strcmp(command, "list-tabs") == 0) {
		handle_list_tabs(client);
	} else if (strncmp(command, "focus-tab ", 10) == 0) {
		handle_focus_tab(client, command + 10);
	} else if (strncmp(command, "close-tab ", 10) == 0) {
		if (strncmp(command + 10, "--force ", 8) == 0) {
			handle_close_tab(client, command + 18, true);
		} else {
			handle_close_tab(client, command + 10, false);
		}
	} else if (strncmp(command, "new-tab -- ", 10) == 0) {
		handle_new_tab(client, command + 10);
	} else if (strcmp(command, "show-launcher") == 0) {
		handle_show_launcher(client);
	} else {
		control_client_send(client, "ERROR Unknown command\n");
	}
}

static void
control_client_destroy(struct cg_control_client *client)
{
	if (client->event_source) {
		wl_event_source_remove(client->event_source);
	}
	if (client->fd >= 0) {
		close(client->fd);
	}
	wl_list_remove(&client->link);
	free(client);
}

static int
handle_client_data(int fd, uint32_t mask, void *data)
{
	struct cg_control_client *client = data;

	if (mask & (WL_EVENT_ERROR | WL_EVENT_HANGUP)) {
		control_client_destroy(client);
		return 0;
	}

	/* Read data into buffer */
	ssize_t n = read(fd, client->buffer + client->buffer_len,
			CONTROL_BUFFER_SIZE - client->buffer_len);
	if (n <= 0) {
		if (n < 0) {
			wlr_log_errno(WLR_ERROR, "Error reading from client");
		}
		control_client_destroy(client);
		return 0;
	}

	client->buffer_len += n;

	/* Process complete lines (commands terminated by \n) */
	char *newline;
	while ((newline = memchr(client->buffer, '\n', client->buffer_len)) != NULL) {
		*newline = '\0'; /* Terminate the command */

		/* Process command */
		process_command(client, client->buffer);

		/* Move remaining data to start of buffer */
		size_t remaining = client->buffer_len - (newline - client->buffer + 1);
		memmove(client->buffer, newline + 1, remaining);
		client->buffer_len = remaining;
	}

	/* Check if buffer is full */
	if (client->buffer_len >= CONTROL_BUFFER_SIZE) {
		wlr_log(WLR_ERROR, "Client buffer overflow, disconnecting");
		control_client_destroy(client);
	}

	return 0;
}

static int
handle_socket_event(int fd, uint32_t mask, void *data)
{
	struct cg_control_server *control = data;

	if (mask & WL_EVENT_ERROR) {
		wlr_log(WLR_ERROR, "Error on control socket");
		return 0;
	}

	/* Accept incoming connection */
	int client_fd = accept(fd, NULL, NULL);
	if (client_fd < 0) {
		wlr_log_errno(WLR_ERROR, "Failed to accept control connection");
		return 0;
	}

	/* Create client structure */
	struct cg_control_client *client = calloc(1, sizeof(struct cg_control_client));
	if (!client) {
		wlr_log_errno(WLR_ERROR, "Failed to allocate client structure");
		close(client_fd);
		return 0;
	}

	client->control = control;
	client->fd = client_fd;
	client->buffer_len = 0;

	set_nonblock(client_fd);

	/* Add to event loop */
	struct wl_event_loop *event_loop =
		wl_display_get_event_loop(control->server->wl_display);
	client->event_source = wl_event_loop_add_fd(event_loop, client_fd,
						   WL_EVENT_READABLE, handle_client_data, client);
	if (!client->event_source) {
		wlr_log(WLR_ERROR, "Failed to add client fd to event loop");
		close(client_fd);
		free(client);
		return 0;
	}

	wl_list_insert(&control->clients, &client->link);
	wlr_log(WLR_DEBUG, "New control client connected");

	return 0;
}

struct cg_control_server *
control_server_create(struct cg_server *server)
{
	if (!server) {
		wlr_log(WLR_ERROR, "server is NULL");
		return NULL;
	}

	struct cg_control_server *control = calloc(1, sizeof(struct cg_control_server));
	if (!control) {
		wlr_log_errno(WLR_ERROR, "Failed to allocate control server");
		return NULL;
	}

	control->server = server;
	wl_list_init(&control->clients);

	/* Get PID for socket path */
	pid_t pid = getpid();

	/* Create socket directory */
	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (!runtime_dir) {
		wlr_log(WLR_ERROR, "XDG_RUNTIME_DIR not set");
		free(control);
		return NULL;
	}

	/* Create waymux directory */
	char socket_dir[PATH_MAX];
	snprintf(socket_dir, sizeof(socket_dir), "%s/waymux", runtime_dir);
	if (mkdir(socket_dir, 0755) != 0 && errno != EEXIST) {
		wlr_log_errno(WLR_ERROR, "Failed to create waymux directory");
		free(control);
		return NULL;
	}

	/* Create socket path */
	control->socket_path = calloc(PATH_MAX, sizeof(char));
	if (!control->socket_path) {
		wlr_log_errno(WLR_ERROR, "Failed to allocate socket path");
		rmdir(socket_dir);
		free(control);
		return NULL;
	}
	snprintf(control->socket_path, PATH_MAX, "%s/waymux/%d.sock", runtime_dir, pid);

	/* Remove existing socket if present */
	unlink(control->socket_path);

	/* Create Unix domain socket */
	control->socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (control->socket_fd < 0) {
		wlr_log_errno(WLR_ERROR, "Failed to create control socket");
		free(control->socket_path);
		rmdir(socket_dir);
		free(control);
		return NULL;
	}

	/* Set socket as non-blocking */
	set_nonblock(control->socket_fd);

	/* Bind socket to path */
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, control->socket_path, sizeof(addr.sun_path) - 1);

	if (bind(control->socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		wlr_log_errno(WLR_ERROR, "Failed to bind control socket");
		close(control->socket_fd);
		free(control->socket_path);
		rmdir(socket_dir);
		free(control);
		return NULL;
	}

	/* Listen for connections */
	if (listen(control->socket_fd, 5) < 0) {
		wlr_log_errno(WLR_ERROR, "Failed to listen on control socket");
		close(control->socket_fd);
		unlink(control->socket_path);
		free(control->socket_path);
		rmdir(socket_dir);
		free(control);
		return NULL;
	}

	/* Add to event loop */
	struct wl_event_loop *event_loop = wl_display_get_event_loop(server->wl_display);
	control->event_source = wl_event_loop_add_fd(event_loop, control->socket_fd,
						    WL_EVENT_READABLE, handle_socket_event, control);
	if (!control->event_source) {
		wlr_log(WLR_ERROR, "Failed to add control socket to event loop");
		close(control->socket_fd);
		unlink(control->socket_path);
		free(control->socket_path);
		rmdir(socket_dir);
		free(control);
		return NULL;
	}

	wlr_log(WLR_INFO, "Control server listening on %s", control->socket_path);

	return control;
}

void
control_server_destroy(struct cg_control_server *control)
{
	if (!control) {
		return;
	}

	/* Close all clients */
	struct cg_control_client *client, *client_tmp;
	wl_list_for_each_safe(client, client_tmp, &control->clients, link) {
		control_client_destroy(client);
	}

	if (control->event_source) {
		wl_event_source_remove(control->event_source);
	}
	if (control->socket_fd >= 0) {
		close(control->socket_fd);
	}
	if (control->socket_path) {
		unlink(control->socket_path);
		free(control->socket_path);

		/* Try to remove directory if empty */
		const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
		if (runtime_dir) {
			char socket_dir[PATH_MAX];
			snprintf(socket_dir, sizeof(socket_dir), "%s/waymux", runtime_dir);
			rmdir(socket_dir);
		}
	}

	free(control);
}
