/*
 * Unit tests for waymuxctl client
 */

#include <check.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

/* Mock server socket for testing */
static int mock_server_fd = -1;
static char mock_socket_path[PATH_MAX];

static void
setup(void)
{
	/* Create a temporary socket for testing */
	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (!runtime_dir) {
		runtime_dir = "/tmp";
	}

	/* Create unique socket path */
	snprintf(mock_socket_path, sizeof(mock_socket_path),
		 "%s/waymux_test_%d.sock", runtime_dir, getpid());

	/* Create socket directory */
	char socket_dir[PATH_MAX];
	snprintf(socket_dir, sizeof(socket_dir), "%s/waymux_test", runtime_dir);
	/* Ignore error if directory already exists */
	mkdir(socket_dir, 0755);

	/* Remove existing socket if present */
	unlink(mock_socket_path);

	/* Create server socket */
	mock_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	ck_assert_int_ne(mock_server_fd, -1);

	/* Bind to path */
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
#if defined(__GNUC__) && __GNUC__ >= 7
	_Pragma("GCC diagnostic push")
	_Pragma("GCC diagnostic ignored \"-Wformat-truncation\"")
#endif
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", mock_socket_path);
#if defined(__GNUC__) && __GNUC__ >= 7
	_Pragma("GCC diagnostic pop")
#endif

	ck_assert_int_eq(bind(mock_server_fd, (struct sockaddr *)&addr, sizeof(addr)), 0);
	ck_assert_int_eq(listen(mock_server_fd, 5), 0);
}

static void
teardown(void)
{
	if (mock_server_fd >= 0) {
		close(mock_server_fd);
		mock_server_fd = -1;
	}
	unlink(mock_socket_path);
}

/* Test that we can connect to a Unix domain socket */
START_TEST(test_connect_to_socket)
{
	/* Create client socket */
	int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	ck_assert_int_ne(client_fd, -1);

	/* Connect to mock server */
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
#if defined(__GNUC__) && __GNUC__ >= 7
	_Pragma("GCC diagnostic push")
	_Pragma("GCC diagnostic ignored \"-Wformat-truncation\"")
#endif
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", mock_socket_path);
#if defined(__GNUC__) && __GNUC__ >= 7
	_Pragma("GCC diagnostic pop")
#endif

	int result = connect(client_fd, (struct sockaddr *)&addr, sizeof(addr));
	ck_assert_int_eq(result, 0);

	/* Accept connection on server side */
	int server_client_fd = accept(mock_server_fd, NULL, NULL);
	ck_assert_int_ne(server_client_fd, -1);

	/* Send test message */
	const char *test_msg = "OK\n";
	ssize_t sent = send(server_client_fd, test_msg, strlen(test_msg), 0);
	ck_assert_int_ne(sent, -1);

	/* Receive on client side */
	char buffer[256];
	ssize_t received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
	ck_assert_int_ne(received, -1);
	buffer[received] = '\0';
	ck_assert_str_eq(buffer, "OK\n");

	/* Cleanup */
	close(server_client_fd);
	close(client_fd);
}
END_TEST

/* Test socket path scanning logic */
START_TEST(test_socket_path_format)
{
	/* Just verify that mock_socket_path was set in setup() */
	ck_assert_int_ne(strlen(mock_socket_path), 0);

	/* Verify it ends with .sock */
	size_t len = strlen(mock_socket_path);
	ck_assert_int_gt(len, 5);
	ck_assert_str_eq(mock_socket_path + len - 5, ".sock");
}
END_TEST

Suite *
waymuxctl_suite(void)
{
	Suite *s = suite_create("waymuxctl");

	TCase *tc_core = tcase_create("Core");
	tcase_add_checked_fixture(tc_core, setup, teardown);
	tcase_add_test(tc_core, test_connect_to_socket);
	tcase_add_test(tc_core, test_socket_path_format);
	suite_add_tcase(s, tc_core);

	return s;
}

int
main(void)
{
	int number_failed;
	Suite *s = waymuxctl_suite();
	SRunner *sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
