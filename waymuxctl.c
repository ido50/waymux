/*
 * waymuxctl - Control client for WayMux
 *
 * Communicates with waymux control server via Unix domain socket
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define CONTROL_BUFFER_SIZE 4096

/* Find and connect to waymux control socket
 * Searches XDG_RUNTIME_DIR/waymux directory for .sock files
 * Returns socket fd on success, -1 on failure
 */
static int
connect_to_waymux(void)
{
	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (!runtime_dir) {
		fprintf(stderr, "ERROR: XDG_RUNTIME_DIR not set\n");
		return -1;
	}

	/* Build socket directory path */
	char socket_dir[PATH_MAX];
	int len = snprintf(socket_dir, sizeof(socket_dir), "%s/waymux", runtime_dir);
	if (len < 0 || (size_t)len >= sizeof(socket_dir)) {
		fprintf(stderr, "ERROR: Socket path too long\n");
		return -1;
	}

	/* Check for WAYMUX_PID environment variable first */
	const char *waymux_pid = getenv("WAYMUX_PID");
	char socket_path[PATH_MAX];

	if (waymux_pid) {
		len = snprintf(socket_path, sizeof(socket_path), "%s/%s.sock", socket_dir, waymux_pid);
		if (len < 0 || (size_t)len >= sizeof(socket_path)) {
			fprintf(stderr, "ERROR: Socket path too long\n");
			return -1;
		}
	} else {
		/* Scan directory for .sock files */
		DIR *dir = opendir(socket_dir);
		if (!dir) {
			fprintf(stderr, "ERROR: No waymux socket directory found at %s\n", socket_dir);
			return -1;
		}

		bool found = false;
		struct dirent *entry;

		while ((entry = readdir(dir)) != NULL) {
			/* Check if entry ends with .sock */
			size_t name_len = strlen(entry->d_name);
			if (name_len > 5 && strcmp(entry->d_name + name_len - 5, ".sock") == 0) {
				len = snprintf(socket_path, sizeof(socket_path), "%s/%s", socket_dir, entry->d_name);
				if (len < 0 || (size_t)len >= sizeof(socket_path)) {
					closedir(dir);
					fprintf(stderr, "ERROR: Socket path too long\n");
					return -1;
				}
				found = true;
				break;
			}
		}

		closedir(dir);

		if (!found) {
			fprintf(stderr, "ERROR: No waymux socket found in %s\n", socket_dir);
			return -1;
		}
	}

	/* Create socket */
	int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		perror("ERROR: Failed to create socket");
		return -1;
	}

	/* Connect to socket */
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

	if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("ERROR: Failed to connect to waymux socket");
		close(sock_fd);
		return -1;
	}

	return sock_fd;
}

/* Send command to waymux and read response
 * Returns 0 on success, -1 on failure
 * Response is written to stdout
 */
static int
send_command(const char *command)
{
	int sock_fd = connect_to_waymux();
	if (sock_fd < 0) {
		return -1;
	}

	/* Send command */
	size_t cmd_len = strlen(command);
	char *cmd_with_newline = malloc(cmd_len + 2);
	if (!cmd_with_newline) {
		fprintf(stderr, "ERROR: Out of memory\n");
		close(sock_fd);
		return -1;
	}

	snprintf(cmd_with_newline, cmd_len + 2, "%s\n", command);

	ssize_t sent = send(sock_fd, cmd_with_newline, cmd_len + 1, 0);
	free(cmd_with_newline);

	if (sent < 0) {
		perror("ERROR: Failed to send command");
		close(sock_fd);
		return -1;
	}

	/* Read response */
	char buffer[CONTROL_BUFFER_SIZE];
	ssize_t n;
	bool first_line = true;

	while ((n = recv(sock_fd, buffer, CONTROL_BUFFER_SIZE - 1, 0)) > 0) {
		buffer[n] = '\0';

		/* Skip the protocol status line (e.g., "OK 1") */
		if (first_line) {
			char *newline = strchr(buffer, '\n');
			if (newline) {
				/* Print everything after the first line */
				fwrite(newline + 1, 1, n - (newline - buffer + 1), stdout);
				first_line = false;
			}
		} else {
			fwrite(buffer, 1, n, stdout);
		}
	}

	if (n < 0) {
		perror("ERROR: Failed to read response");
		close(sock_fd);
		return -1;
	}

	close(sock_fd);
	return 0;
}

static void
usage(const char *prog_name)
{
	fprintf(stderr, "Usage: %s <command> [args]\n", prog_name);
	fprintf(stderr, "\nCommands:\n");
	fprintf(stderr, "  list-tabs              List all tabs\n");
	fprintf(stderr, "  focus-tab <NUM>        Switch to tab NUM\n");
	fprintf(stderr, "  close-tab [--force] <NUM>  Close tab NUM\n");
	fprintf(stderr, "  new-tab -- <CMD>       Create new tab running CMD\n");
	fprintf(stderr, "\n");
}

int
main(int argc, char *argv[])
{
	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	const char *command = argv[1];

	/* Build command string for server */
	char server_cmd[CONTROL_BUFFER_SIZE];

	if (strcmp(command, "list-tabs") == 0) {
		snprintf(server_cmd, sizeof(server_cmd), "list-tabs");
		return send_command(server_cmd) == 0 ? 0 : 1;

	} else if (strcmp(command, "focus-tab") == 0) {
		if (argc < 3) {
			fprintf(stderr, "ERROR: Missing tab index\n");
			usage(argv[0]);
			return 1;
		}
		snprintf(server_cmd, sizeof(server_cmd), "focus-tab %s", argv[2]);
		return send_command(server_cmd) == 0 ? 0 : 1;

	} else if (strcmp(command, "close-tab") == 0) {
		if (argc < 3) {
			fprintf(stderr, "ERROR: Missing tab index\n");
			usage(argv[0]);
			return 1;
		}

		int arg_idx = 2;
		bool force = false;

		/* Check for --force flag */
		if (strcmp(argv[arg_idx], "--force") == 0) {
			force = true;
			arg_idx++;
		}

		if (arg_idx >= argc) {
			fprintf(stderr, "ERROR: Missing tab index\n");
			usage(argv[0]);
			return 1;
		}

		if (force) {
			snprintf(server_cmd, sizeof(server_cmd), "close-tab --force %s", argv[arg_idx]);
		} else {
			snprintf(server_cmd, sizeof(server_cmd), "close-tab %s", argv[arg_idx]);
		}
		return send_command(server_cmd) == 0 ? 0 : 1;

	} else if (strcmp(command, "new-tab") == 0) {
		if (argc < 4 || strcmp(argv[2], "--") != 0) {
			fprintf(stderr, "ERROR: new-tab requires -- separator\n");
			fprintf(stderr, "Usage: %s new-tab -- <command> [args...]\n", argv[0]);
			return 1;
		}

		/* Build command string from argv[3+] */
		size_t offset = snprintf(server_cmd, sizeof(server_cmd), "new-tab --");
		for (int i = 3; i < argc && offset < sizeof(server_cmd) - 2; i++) {
			offset += snprintf(server_cmd + offset, sizeof(server_cmd) - offset, " %s", argv[i]);
		}

		return send_command(server_cmd) == 0 ? 0 : 1;

	} else {
		fprintf(stderr, "ERROR: Unknown command '%s'\n", command);
		usage(argv[0]);
		return 1;
	}

	return 0;
}
