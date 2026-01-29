/*
 * WayMux: A Wayland multiplexer.
 *
 * Copyright (C) 2025 Ido Perlmuter
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200809L

#include <check.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <wayland-server-core.h>

#include "server.h"
#include "control.h"

/* Minimal server setup for testing */
static struct cg_server *
create_test_server(void)
{
	struct cg_server *server = calloc(1, sizeof(struct cg_server));
	if (!server) {
		return NULL;
	}

	/* Initialize lists */
	wl_list_init(&server->views);
	wl_list_init(&server->outputs);
	wl_list_init(&server->tabs);
	server->active_tab = NULL;

	/* Create minimal Wayland display for event loop */
	server->wl_display = wl_display_create();
	if (!server->wl_display) {
		free(server);
		return NULL;
	}

	return server;
}

static void
destroy_test_server(struct cg_server *server)
{
	if (server->wl_display) {
		wl_display_destroy(server->wl_display);
	}
	free(server);
}

/* Test: control_server_create and destroy */
START_TEST(test_control_create_destroy)
{
	struct cg_server *server = create_test_server();
	ck_assert_ptr_nonnull(server);

	struct cg_control_server *control = control_server_create(server);
	ck_assert_ptr_nonnull(control);
	ck_assert_ptr_eq(control->server, server);
	ck_assert_int_gt(control->socket_fd, 0);
	ck_assert_ptr_nonnull(control->socket_path);

	/* Verify socket path format */
	char *pid_str = strrchr(control->socket_path, '/');
	ck_assert_ptr_nonnull(pid_str);
	pid_str++; /* Skip the '/' */

	/* Verify it ends with .sock */
	const char *sock_ext = strstr(pid_str, ".sock");
	ck_assert_ptr_nonnull(sock_ext);

	/* Clean up */
	control_server_destroy(control);
	destroy_test_server(server);

	/* Verify socket was removed */
	struct stat st;
	ck_assert_int_ne(stat(control->socket_path, &st), 0);
}
END_TEST

/* Test: control_server handles NULL server gracefully */
START_TEST(test_control_null_server)
{
	struct cg_control_server *control = control_server_create(NULL);
	ck_assert_ptr_null(control);
}
END_TEST

/* Test: control_server_destroy handles NULL gracefully */
START_TEST(test_control_destroy_null)
{
	/* Should not crash */
	control_server_destroy(NULL);
}
END_TEST

/* Test: Verify socket is created in correct location */
START_TEST(test_control_socket_path)
{
	struct cg_server *server = create_test_server();
	ck_assert_ptr_nonnull(server);

	struct cg_control_server *control = control_server_create(server);
	ck_assert_ptr_nonnull(control);

	/* Verify path starts with XDG_RUNTIME_DIR/waymux/ */
	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	ck_assert_ptr_nonnull(runtime_dir);

	char expected_prefix[PATH_MAX];
	snprintf(expected_prefix, sizeof(expected_prefix), "%s/waymux/", runtime_dir);

	int cmp = strncmp(control->socket_path, expected_prefix, strlen(expected_prefix));
	ck_assert_int_eq(cmp, 0);

	control_server_destroy(control);
	destroy_test_server(server);
}
END_TEST

/* Test: Socket permissions and accessibility */
START_TEST(test_control_socket_permissions)
{
	struct cg_server *server = create_test_server();
	ck_assert_ptr_nonnull(server);

	struct cg_control_server *control = control_server_create(server);
	ck_assert_ptr_nonnull(control);

	/* Verify socket file exists and is a socket */
	struct stat st;
	ck_assert_int_eq(stat(control->socket_path, &st), 0);
	ck_assert(S_ISSOCK(st.st_mode));

	control_server_destroy(control);
	destroy_test_server(server);
}
END_TEST

/* Test: Client can connect to socket */
START_TEST(test_control_client_connect)
{
	struct cg_server *server = create_test_server();
	ck_assert_ptr_nonnull(server);

	struct cg_control_server *control = control_server_create(server);
	ck_assert_ptr_nonnull(control);

	/* Create a client socket */
	int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	ck_assert_int_gt(client_fd, 0);

	/* Connect to control socket */
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, control->socket_path, sizeof(addr.sun_path) - 1);

	int result = connect(client_fd, (struct sockaddr *)&addr, sizeof(addr));
	ck_assert_int_eq(result, 0);

	/* Clean up */
	close(client_fd);
	control_server_destroy(control);
	destroy_test_server(server);
}
END_TEST

/* Test: Multiple clients can connect */
START_TEST(test_control_multiple_clients)
{
	struct cg_server *server = create_test_server();
	ck_assert_ptr_nonnull(server);

	struct cg_control_server *control = control_server_create(server);
	ck_assert_ptr_nonnull(control);

	/* Create multiple client sockets */
	int client_fds[3];
	for (int i = 0; i < 3; i++) {
		client_fds[i] = socket(AF_UNIX, SOCK_STREAM, 0);
		ck_assert_int_gt(client_fds[i], 0);

		struct sockaddr_un addr;
		memset(&addr, 0, sizeof(addr));
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, control->socket_path, sizeof(addr.sun_path) - 1);

		int result = connect(client_fds[i], (struct sockaddr *)&addr, sizeof(addr));
		ck_assert_int_eq(result, 0);
	}

	/* Clean up */
	for (int i = 0; i < 3; i++) {
		close(client_fds[i]);
	}
	control_server_destroy(control);
	destroy_test_server(server);
}
END_TEST

Suite *
control_suite(void)
{
	Suite *s = suite_create("control");

	TCase *tc_core = tcase_create("Core");
	tcase_add_test(tc_core, test_control_create_destroy);
	tcase_add_test(tc_core, test_control_null_server);
	tcase_add_test(tc_core, test_control_destroy_null);
	tcase_add_test(tc_core, test_control_socket_path);
	tcase_add_test(tc_core, test_control_socket_permissions);
	suite_add_tcase(s, tc_core);

	TCase *tc_network = tcase_create("Network");
	tcase_add_test(tc_network, test_control_client_connect);
	tcase_add_test(tc_network, test_control_multiple_clients);
	suite_add_tcase(s, tc_network);

	return s;
}

int
main(void)
{
	int number_failed;
	Suite *s = control_suite();
	SRunner *sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
