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
#include <tomlc17.h>

#define CONTROL_BUFFER_SIZE 4096
#define REGISTRY_DIR "/waymux/registry"

/* Instance name to connect to (NULL = use default or auto-detect) */
static const char *target_instance = NULL;

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

	/* Check for WAYMUX_INSTANCE environment variable first, then --instance flag */
	const char *instance_name = getenv("WAYMUX_INSTANCE");
	if (!instance_name && target_instance) {
		instance_name = target_instance;
	}
	if (!instance_name) {
		instance_name = "default";
	}

	char socket_path[PATH_MAX];
	len = snprintf(socket_path, sizeof(socket_path), "%s/%s.sock", socket_dir, instance_name);
	if (len < 0 || (size_t)len >= sizeof(socket_path)) {
		fprintf(stderr, "ERROR: Socket path too long\n");
		return -1;
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

/* List all running instances from the registry
 * Returns 0 on success, -1 on failure
 */
static int
list_instances(void)
{
	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (!runtime_dir) {
		fprintf(stderr, "ERROR: XDG_RUNTIME_DIR not set\n");
		return -1;
	}

	/* Build registry directory path */
	char registry_dir[PATH_MAX];
	int len = snprintf(registry_dir, sizeof(registry_dir), "%s%s", runtime_dir, REGISTRY_DIR);
	if (len < 0 || (size_t)len >= sizeof(registry_dir)) {
		fprintf(stderr, "ERROR: Registry path too long\n");
		return -1;
	}

	/* Open registry directory */
	DIR *dir = opendir(registry_dir);
	if (!dir) {
		if (errno == ENOENT) {
			/* No registry directory means no instances */
			printf("No running instances\n");
			return 0;
		}
		perror("ERROR: Failed to open registry directory");
		return -1;
	}

	struct dirent *entry;
	int instance_count = 0;

	printf("Running instances:\n");

	while ((entry = readdir(dir)) != NULL) {
		/* Skip . and .. */
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
			continue;
		}

		/* Check if it's a .toml file */
		size_t name_len = strlen(entry->d_name);
		if (name_len < 6 || strcmp(entry->d_name + name_len - 5, ".toml") != 0) {
			continue;
		}

		/* Build full path */
		char file_path[PATH_MAX];
		len = snprintf(file_path, sizeof(file_path), "%s/%s", registry_dir, entry->d_name);
		if (len < 0 || (size_t)len >= sizeof(file_path)) {
			continue;
		}

		/* Parse the TOML file */
		toml_result_t result = toml_parse_file_ex(file_path);
		if (!result.ok) {
			fprintf(stderr, "Warning: Failed to parse %s\n", file_path);
			continue;
		}

		/* Extract instance name from filename (remove .toml extension) */
		char instance_name[PATH_MAX];
		strncpy(instance_name, entry->d_name, name_len - 5);
		instance_name[name_len - 5] = '\0';

		/* Get PID */
		toml_datum_t pid_datum = toml_get(result.toptab, "pid");
		int pid = -1;
		if (pid_datum.type == TOML_INT64) {
			pid = (int)pid_datum.u.int64;
		}

		/* Get profile (optional) */
		toml_datum_t profile_datum = toml_get(result.toptab, "profile");
		const char *profile = NULL;
		if (profile_datum.type == TOML_STRING && profile_datum.u.s) {
			profile = profile_datum.u.s;
		}

		/* Print instance info */
		printf("  %s", instance_name);
		if (profile) {
			printf(" (profile: %s)", profile);
		}
		if (pid > 0) {
			printf(" [pid: %d]", pid);
		}
		printf("\n");

		toml_free(result);
		instance_count++;
	}

	closedir(dir);

	if (instance_count == 0) {
		printf("  (none)\n");
	}

	return 0;
}

static void
usage(const char *prog_name)
{
	fprintf(stderr, "Usage: %s [OPTIONS] <command> [args]\n", prog_name);
	fprintf(stderr, "\nOptions:\n");
	fprintf(stderr, "  -i, --instance <NAME>  Target specific instance (default: 'default')\n");
	fprintf(stderr, "\nCommands:\n");
	fprintf(stderr, "  instances              List all running instances\n");
	fprintf(stderr, "  list-tabs              List all tabs\n");
	fprintf(stderr, "  focus-tab <NUM>        Switch to tab NUM\n");
	fprintf(stderr, "  close-tab [--force] <NUM>  Close tab NUM\n");
	fprintf(stderr, "  background <NUM>       Move tab to background (hide from tab bar)\n");
	fprintf(stderr, "  foreground <NUM>       Bring background tab to foreground\n");
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

	/* Parse options */
	int arg_idx = 1;
	while (arg_idx < argc && argv[arg_idx][0] == '-') {
		if (strcmp(argv[arg_idx], "-i") == 0 || strcmp(argv[arg_idx], "--instance") == 0) {
			if (arg_idx + 1 >= argc) {
				fprintf(stderr, "ERROR: %s requires an argument\n", argv[arg_idx]);
				usage(argv[0]);
				return 1;
			}
			target_instance = argv[arg_idx + 1];
			arg_idx += 2;
		} else if (strcmp(argv[arg_idx], "--") == 0) {
			/* Stop option processing */
			arg_idx++;
			break;
		} else if (strcmp(argv[arg_idx], "-h") == 0 || strcmp(argv[arg_idx], "--help") == 0) {
			usage(argv[0]);
			return 0;
		} else {
			fprintf(stderr, "ERROR: Unknown option '%s'\n", argv[arg_idx]);
			usage(argv[0]);
			return 1;
		}
	}

	if (arg_idx >= argc) {
		usage(argv[0]);
		return 1;
	}

	const char *command = argv[arg_idx++];

	/* Build command string for server */
	char server_cmd[CONTROL_BUFFER_SIZE];

	if (strcmp(command, "instances") == 0) {
		return list_instances() == 0 ? 0 : 1;

	} else if (strcmp(command, "list-tabs") == 0) {
		snprintf(server_cmd, sizeof(server_cmd), "list-tabs");
		return send_command(server_cmd) == 0 ? 0 : 1;

	} else if (strcmp(command, "focus-tab") == 0) {
		if (arg_idx >= argc) {
			fprintf(stderr, "ERROR: Missing tab index\n");
			usage(argv[0]);
			return 1;
		}
		snprintf(server_cmd, sizeof(server_cmd), "focus-tab %s", argv[arg_idx]);
		return send_command(server_cmd) == 0 ? 0 : 1;

	} else if (strcmp(command, "close-tab") == 0) {
		if (arg_idx >= argc) {
			fprintf(stderr, "ERROR: Missing tab index\n");
			usage(argv[0]);
			return 1;
		}

		int cmd_arg_idx = arg_idx;
		bool force = false;

		/* Check for --force flag */
		if (cmd_arg_idx < argc && strcmp(argv[cmd_arg_idx], "--force") == 0) {
			force = true;
			cmd_arg_idx++;
		}

		if (cmd_arg_idx >= argc) {
			fprintf(stderr, "ERROR: Missing tab index\n");
			usage(argv[0]);
			return 1;
		}

		if (force) {
			snprintf(server_cmd, sizeof(server_cmd), "close-tab --force %s", argv[cmd_arg_idx]);
		} else {
			snprintf(server_cmd, sizeof(server_cmd), "close-tab %s", argv[cmd_arg_idx]);
		}
		return send_command(server_cmd) == 0 ? 0 : 1;

	} else if (strcmp(command, "background") == 0) {
		if (arg_idx >= argc) {
			fprintf(stderr, "ERROR: Missing tab index\n");
			usage(argv[0]);
			return 1;
		}
		snprintf(server_cmd, sizeof(server_cmd), "background %s", argv[arg_idx]);
		return send_command(server_cmd) == 0 ? 0 : 1;

	} else if (strcmp(command, "foreground") == 0) {
		if (arg_idx >= argc) {
			fprintf(stderr, "ERROR: Missing tab index\n");
			usage(argv[0]);
			return 1;
		}
		snprintf(server_cmd, sizeof(server_cmd), "foreground %s", argv[arg_idx]);
		return send_command(server_cmd) == 0 ? 0 : 1;

	} else if (strcmp(command, "new-tab") == 0) {
		if (arg_idx >= argc || strcmp(argv[arg_idx], "--") != 0) {
			fprintf(stderr, "ERROR: new-tab requires -- separator\n");
			fprintf(stderr, "Usage: %s new-tab -- <command> [args...]\n", argv[0]);
			return 1;
		}

		/* Build command string from argv[arg_idx+1+] */
		size_t offset = snprintf(server_cmd, sizeof(server_cmd), "new-tab --");
		for (int i = arg_idx + 1; i < argc && offset < sizeof(server_cmd) - 2; i++) {
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
