/*
 * Waymux: Profile tests
 *
 * Copyright (C) 2025 Ido Perlmuter
 *
 * See the LICENSE file accompanying this file.
 */

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "profile.h"

static char *test_profile_name = NULL;
static char *test_profile_path = NULL;
static char *saved_cwd = NULL;

static void
setup(void)
{
	/* Save current working directory */
	char cwd[PATH_MAX];
	ck_assert_ptr_nonnull(getcwd(cwd, sizeof(cwd)));
	saved_cwd = strdup(cwd);
	ck_assert_ptr_nonnull(saved_cwd);

	/* Change to the source directory where we can create the test file */
	/* Tests run from build directory, go to parent (source) */
	ck_assert_int_eq(chdir(".."), 0);

	/* Create a test profile file in current directory */
	/* We use a fixed name that profile_load can find via ./name.toml */
	test_profile_name = strdup("waymux_test_profile");
	test_profile_path = strdup("./waymux_test_profile.toml");
	ck_assert_ptr_nonnull(test_profile_name);
	ck_assert_ptr_nonnull(test_profile_path);

	FILE *f = fopen(test_profile_path, "w");
	ck_assert_ptr_nonnull(f);

	fprintf(f, "working_dir = \"/home/user/projects\"\n");
	fprintf(f, "proxy_command = [\"uv\", \"run\"]\n");
	fprintf(f, "\n");
	fprintf(f, "[env]\n");
	fprintf(f, "EDITOR = \"nvim\"\n");
	fprintf(f, "DEBUG = \"1\"\n");
	fprintf(f, "\n");
	fprintf(f, "[[tabs]]\n");
	fprintf(f, "command = \"kitty\"\n");
	fprintf(f, "title = \"Terminal\"\n");
	fprintf(f, "args = [\"-e\", \"nvim\"]\n");
	fprintf(f, "\n");
	fprintf(f, "[[tabs]]\n");
	fprintf(f, "command = \"firefox\"\n");
	fprintf(f, "title = \"Browser\"\n");
	fprintf(f, "args = [\"--new-window\", \"https://example.com\"]\n");
	fprintf(f, "\n");
	fprintf(f, "[[tabs]]\n");
	fprintf(f, "command = \"foot\"\n");
	fprintf(f, "\n");

	fclose(f);
}

static void
setup_background(void)
{
	/* Save current working directory */
	char cwd[PATH_MAX];
	ck_assert_ptr_nonnull(getcwd(cwd, sizeof(cwd)));
	saved_cwd = strdup(cwd);
	ck_assert_ptr_nonnull(saved_cwd);

	/* Change to the source directory where we can create the test file */
	ck_assert_int_eq(chdir(".."), 0);

	/* Create a test profile with background tabs */
	test_profile_name = strdup("waymux_test_profile_bg");
	test_profile_path = strdup("./waymux_test_profile_bg.toml");
	ck_assert_ptr_nonnull(test_profile_name);
	ck_assert_ptr_nonnull(test_profile_path);

	FILE *f = fopen(test_profile_path, "w");
	ck_assert_ptr_nonnull(f);

	fprintf(f, "[[tabs]]\n");
	fprintf(f, "command = \"kitty\"\n");
	fprintf(f, "title = \"Foreground Tab\"\n");
	fprintf(f, "\n");
	fprintf(f, "[[tabs]]\n");
	fprintf(f, "command = \"foot\"\n");
	fprintf(f, "title = \"Background Tab\"\n");
	fprintf(f, "background = true\n");
	fprintf(f, "\n");
	fprintf(f, "[[tabs]]\n");
	fprintf(f, "command = \"firefox\"\n");
	fprintf(f, "title = \"Another Foreground Tab\"\n");
	fprintf(f, "background = false\n");

	fclose(f);
}

static void
teardown(void)
{
	if (test_profile_path) {
		unlink(test_profile_path);
		free(test_profile_path);
		test_profile_path = NULL;
	}
	if (test_profile_name) {
		free(test_profile_name);
		test_profile_name = NULL;
	}
	/* Restore original working directory */
	if (saved_cwd) {
		(void)chdir(saved_cwd);
		free(saved_cwd);
		saved_cwd = NULL;
	}
}

START_TEST(test_profile_load_valid)
{
	struct profile *profile = profile_load(test_profile_name);
	ck_assert_ptr_nonnull(profile);

	ck_assert_str_eq(profile->name, test_profile_name);
	ck_assert_str_eq(profile->working_dir, "/home/user/projects");
	ck_assert_int_eq(profile->proxy_argc, 2);
	ck_assert_str_eq(profile->proxy_command[0], "uv");
	ck_assert_str_eq(profile->proxy_command[1], "run");

	profile_free(profile);
}
END_TEST

START_TEST(test_profile_env_vars)
{
	struct profile *profile = profile_load(test_profile_name);
	ck_assert_ptr_nonnull(profile);

	ck_assert_int_eq(profile->env_count, 2);
	ck_assert_str_eq(profile->env_vars[0].key, "EDITOR");
	ck_assert_str_eq(profile->env_vars[0].value, "nvim");
	ck_assert_str_eq(profile->env_vars[1].key, "DEBUG");
	ck_assert_str_eq(profile->env_vars[1].value, "1");

	profile_free(profile);
}
END_TEST

START_TEST(test_profile_tabs)
{
	struct profile *profile = profile_load(test_profile_name);
	ck_assert_ptr_nonnull(profile);

	ck_assert_int_eq(profile->tab_count, 3);

	/* First tab */
	ck_assert_str_eq(profile->tabs[0].command, "kitty");
	ck_assert_str_eq(profile->tabs[0].title, "Terminal");
	ck_assert_int_eq(profile->tabs[0].argc, 2);
	ck_assert_str_eq(profile->tabs[0].args[0], "-e");
	ck_assert_str_eq(profile->tabs[0].args[1], "nvim");
	ck_assert_ptr_null(profile->tabs[0].args[2]);

	/* Second tab */
	ck_assert_str_eq(profile->tabs[1].command, "firefox");
	ck_assert_str_eq(profile->tabs[1].title, "Browser");
	ck_assert_int_eq(profile->tabs[1].argc, 2);

	/* Third tab - no title or args */
	ck_assert_str_eq(profile->tabs[2].command, "foot");
	ck_assert_ptr_null(profile->tabs[2].title);
	ck_assert_int_eq(profile->tabs[2].argc, 0);

	profile_free(profile);
}
END_TEST

START_TEST(test_profile_background_tabs)
{
	struct profile *profile = profile_load(test_profile_name);
	ck_assert_ptr_nonnull(profile);

	ck_assert_int_eq(profile->tab_count, 3);

	/* First tab - no background field (defaults to false) */
	ck_assert_str_eq(profile->tabs[0].command, "kitty");
	ck_assert_str_eq(profile->tabs[0].title, "Foreground Tab");
	ck_assert_int_eq(profile->tabs[0].background, false);

	/* Second tab - background = true */
	ck_assert_str_eq(profile->tabs[1].command, "foot");
	ck_assert_str_eq(profile->tabs[1].title, "Background Tab");
	ck_assert_int_eq(profile->tabs[1].background, true);

	/* Third tab - background = false (explicit) */
	ck_assert_str_eq(profile->tabs[2].command, "firefox");
	ck_assert_str_eq(profile->tabs[2].title, "Another Foreground Tab");
	ck_assert_int_eq(profile->tabs[2].background, false);

	profile_free(profile);
}
END_TEST

START_TEST(test_profile_file_not_found)
{
	struct profile *profile = profile_load("nonexistent_profile_xyz");
	ck_assert_ptr_null(profile);
}
END_TEST

START_TEST(test_profile_null_name)
{
	struct profile *profile = profile_load(NULL);
	ck_assert_ptr_null(profile);
}
END_TEST

Suite *
profile_suite(void)
{
	Suite *s;
	TCase *tc_core;

	s = suite_create("Profile");

	tc_core = tcase_create("Core");
	tcase_add_checked_fixture(tc_core, setup, teardown);
	tcase_add_test(tc_core, test_profile_load_valid);
	tcase_add_test(tc_core, test_profile_env_vars);
	tcase_add_test(tc_core, test_profile_tabs);
	suite_add_tcase(s, tc_core);

	TCase *tc_background = tcase_create("Background");
	tcase_add_checked_fixture(tc_background, setup_background, teardown);
	tcase_add_test(tc_background, test_profile_background_tabs);
	suite_add_tcase(s, tc_background);

	TCase *tc_errors = tcase_create("Errors");
	tcase_add_test(tc_errors, test_profile_file_not_found);
	tcase_add_test(tc_errors, test_profile_null_name);
	suite_add_tcase(s, tc_errors);

	return s;
}

int
main(void)
{
	int number_failed;
	Suite *s;
	SRunner *sr;

	s = profile_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
