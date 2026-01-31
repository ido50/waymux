/*
 * WayMux: A Wayland multiplexer.
 *
 * Copyright (C) 2025 Ido Perlmuter
 *
 * See the LICENSE file accompanying this file.
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wlr/types/wlr_keyboard.h>
#include <xkbcommon/xkbcommon.h>
#include "../waymux_config.h"
#include "../keybinding.h"

/* Helper to create a temporary config file */
static char *
create_temp_config(const char *content)
{
	char template[] = "/tmp/waymux_config_test_XXXXXX";
	int fd = mkstemp(template);
	if (fd < 0) {
		return NULL;
	}

	if (write(fd, content, strlen(content)) != (ssize_t)strlen(content)) {
		close(fd);
		unlink(template);
		return NULL;
	}

	close(fd);
	return strdup(template);
}

START_TEST(test_load_defaults_no_file)
{
	/* Load with a non-existent path - should return defaults */
	struct waymux_config *config = waymux_config_load("/nonexistent/path/config.toml");

	/* Should not return NULL - returns defaults when file not found */
	ck_assert_msg(config != NULL, "Should return defaults when file not found");

	/* Check that all keybindings are set to defaults */
	ck_assert(config->next_tab != NULL);
	ck_assert(config->prev_tab != NULL);
	ck_assert(config->close_tab != NULL);
	ck_assert(config->open_launcher != NULL);
	ck_assert(config->toggle_background != NULL);
	ck_assert(config->show_background_dialog != NULL);

	/* Check defaults: Super+K for next_tab */
	ck_assert_uint_eq(config->next_tab->modifiers, WLR_MODIFIER_LOGO);
	ck_assert_uint_eq(config->next_tab->keysym, XKB_KEY_k);

	/* Check defaults: Super+J for prev_tab */
	ck_assert_uint_eq(config->prev_tab->modifiers, WLR_MODIFIER_LOGO);
	ck_assert_uint_eq(config->prev_tab->keysym, XKB_KEY_j);

	/* Check defaults: Super+D for close_tab */
	ck_assert_uint_eq(config->close_tab->modifiers, WLR_MODIFIER_LOGO);
	ck_assert_uint_eq(config->close_tab->keysym, XKB_KEY_d);

	/* Check defaults: Super+N for open_launcher */
	ck_assert_uint_eq(config->open_launcher->modifiers, WLR_MODIFIER_LOGO);
	ck_assert_uint_eq(config->open_launcher->keysym, XKB_KEY_n);

	/* Check defaults: Super+B for toggle_background */
	ck_assert_uint_eq(config->toggle_background->modifiers, WLR_MODIFIER_LOGO);
	ck_assert_uint_eq(config->toggle_background->keysym, XKB_KEY_b);

	/* Check defaults: Super+Shift+B for show_background_dialog */
	ck_assert_uint_eq(config->show_background_dialog->modifiers, WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT);
	ck_assert_uint_eq(config->show_background_dialog->keysym, XKB_KEY_b);

	waymux_config_free(config);
}
END_TEST

START_TEST(test_load_valid_config)
{
	const char *config_content =
		"[keybindings]\n"
		"next_tab = \"Ctrl+K\"\n"
		"prev_tab = \"Ctrl+J\"\n"
		"close_tab = \"Ctrl+D\"\n"
		"open_launcher = \"Ctrl+N\"\n"
		"toggle_background = \"Ctrl+B\"\n"
		"show_background_dialog = \"Ctrl+Shift+B\"\n";

	char *path = create_temp_config(config_content);
	ck_assert_msg(path != NULL, "Failed to create temp config file");

	struct waymux_config *config = waymux_config_load(path);
	unlink(path);
	free(path);

	ck_assert_msg(config != NULL, "Failed to load config");

	/* Check that keybindings match the config */
	ck_assert_uint_eq(config->next_tab->modifiers, WLR_MODIFIER_CTRL);
	ck_assert_uint_eq(config->next_tab->keysym, XKB_KEY_k);

	ck_assert_uint_eq(config->prev_tab->modifiers, WLR_MODIFIER_CTRL);
	ck_assert_uint_eq(config->prev_tab->keysym, XKB_KEY_j);

	ck_assert_uint_eq(config->close_tab->modifiers, WLR_MODIFIER_CTRL);
	ck_assert_uint_eq(config->close_tab->keysym, XKB_KEY_d);

	ck_assert_uint_eq(config->open_launcher->modifiers, WLR_MODIFIER_CTRL);
	ck_assert_uint_eq(config->open_launcher->keysym, XKB_KEY_n);

	ck_assert_uint_eq(config->toggle_background->modifiers, WLR_MODIFIER_CTRL);
	ck_assert_uint_eq(config->toggle_background->keysym, XKB_KEY_b);

	ck_assert_uint_eq(config->show_background_dialog->modifiers, WLR_MODIFIER_CTRL | WLR_MODIFIER_SHIFT);
	ck_assert_uint_eq(config->show_background_dialog->keysym, XKB_KEY_b);

	waymux_config_free(config);
}
END_TEST

START_TEST(test_load_partial_config_uses_defaults)
{
	const char *config_content =
		"[keybindings]\n"
		"next_tab = \"Alt+K\"\n"
		"prev_tab = \"Alt+J\"\n";
		/* Other bindings not specified - should use defaults */

	char *path = create_temp_config(config_content);
	ck_assert_msg(path != NULL, "Failed to create temp config file");

	struct waymux_config *config = waymux_config_load(path);
	unlink(path);
	free(path);

	ck_assert_msg(config != NULL, "Failed to load config");

	/* Custom bindings */
	ck_assert_uint_eq(config->next_tab->modifiers, WLR_MODIFIER_ALT);
	ck_assert_uint_eq(config->next_tab->keysym, XKB_KEY_k);

	ck_assert_uint_eq(config->prev_tab->modifiers, WLR_MODIFIER_ALT);
	ck_assert_uint_eq(config->prev_tab->keysym, XKB_KEY_j);

	/* Default bindings for unspecified keys */
	ck_assert_uint_eq(config->close_tab->modifiers, WLR_MODIFIER_LOGO);
	ck_assert_uint_eq(config->close_tab->keysym, XKB_KEY_d);

	ck_assert_uint_eq(config->open_launcher->modifiers, WLR_MODIFIER_LOGO);
	ck_assert_uint_eq(config->open_launcher->keysym, XKB_KEY_n);

	ck_assert_uint_eq(config->toggle_background->modifiers, WLR_MODIFIER_LOGO);
	ck_assert_uint_eq(config->toggle_background->keysym, XKB_KEY_b);

	ck_assert_uint_eq(config->show_background_dialog->modifiers, WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT);
	ck_assert_uint_eq(config->show_background_dialog->keysym, XKB_KEY_b);

	waymux_config_free(config);
}
END_TEST

START_TEST(test_load_invalid_keybinding_returns_null)
{
	const char *config_content =
		"[keybindings]\n"
		"next_tab = \"Super+InvalidKeyXYZ\"\n"
		"prev_tab = \"Ctrl+J\"\n";

	char *path = create_temp_config(config_content);
	ck_assert_msg(path != NULL, "Failed to create temp config file");

	struct waymux_config *config = waymux_config_load(path);
	unlink(path);
	free(path);

	/* Invalid keybinding should cause config load to fail */
	ck_assert_msg(config == NULL, "Should return NULL for invalid keybinding");

	waymux_config_free(config); /* Should handle NULL gracefully */
}
END_TEST

START_TEST(test_load_empty_keybindings_section)
{
	const char *config_content =
		"[keybindings]\n";
		/* Empty section - should use all defaults */

	char *path = create_temp_config(config_content);
	ck_assert_msg(path != NULL, "Failed to create temp config file");

	struct waymux_config *config = waymux_config_load(path);
	unlink(path);
	free(path);

	ck_assert_msg(config != NULL, "Failed to load config");

	/* Should all be defaults */
	ck_assert_uint_eq(config->next_tab->modifiers, WLR_MODIFIER_LOGO);
	ck_assert_uint_eq(config->next_tab->keysym, XKB_KEY_k);

	ck_assert_uint_eq(config->prev_tab->modifiers, WLR_MODIFIER_LOGO);
	ck_assert_uint_eq(config->prev_tab->keysym, XKB_KEY_j);

	waymux_config_free(config);
}
END_TEST

START_TEST(test_load_no_keybindings_section)
{
	const char *config_content =
		"# Config file without keybindings section\n"
		"some_other_setting = \"value\"\n";

	char *path = create_temp_config(config_content);
	ck_assert_msg(path != NULL, "Failed to create temp config file");

	struct waymux_config *config = waymux_config_load(path);
	unlink(path);
	free(path);

	ck_assert_msg(config != NULL, "Failed to load config");

	/* Should all be defaults */
	ck_assert_uint_eq(config->next_tab->modifiers, WLR_MODIFIER_LOGO);
	ck_assert_uint_eq(config->next_tab->keysym, XKB_KEY_k);

	waymux_config_free(config);
}
END_TEST

START_TEST(test_get_default_keybindings)
{
	const struct keybinding *kb;

	kb = waymux_config_get_default("next_tab");
	ck_assert_ptr_nonnull(kb);
	ck_assert_uint_eq(kb->modifiers, WLR_MODIFIER_LOGO);
	ck_assert_uint_eq(kb->keysym, XKB_KEY_k);

	kb = waymux_config_get_default("prev_tab");
	ck_assert_ptr_nonnull(kb);
	ck_assert_uint_eq(kb->modifiers, WLR_MODIFIER_LOGO);
	ck_assert_uint_eq(kb->keysym, XKB_KEY_j);

	kb = waymux_config_get_default("close_tab");
	ck_assert_ptr_nonnull(kb);
	ck_assert_uint_eq(kb->modifiers, WLR_MODIFIER_LOGO);
	ck_assert_uint_eq(kb->keysym, XKB_KEY_d);

	kb = waymux_config_get_default("open_launcher");
	ck_assert_ptr_nonnull(kb);
	ck_assert_uint_eq(kb->modifiers, WLR_MODIFIER_LOGO);
	ck_assert_uint_eq(kb->keysym, XKB_KEY_n);

	kb = waymux_config_get_default("toggle_background");
	ck_assert_ptr_nonnull(kb);
	ck_assert_uint_eq(kb->modifiers, WLR_MODIFIER_LOGO);
	ck_assert_uint_eq(kb->keysym, XKB_KEY_b);

	kb = waymux_config_get_default("show_background_dialog");
	ck_assert_ptr_nonnull(kb);
	ck_assert_uint_eq(kb->modifiers, WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT);
	ck_assert_uint_eq(kb->keysym, XKB_KEY_b);

	kb = waymux_config_get_default("invalid_action");
	ck_assert_ptr_null(kb);
}
END_TEST

Suite *
waymux_config_suite(void)
{
	Suite *suite = suite_create("waymux_config");

	TCase *tcase_load = tcase_create("load");
	tcase_add_test(tcase_load, test_load_defaults_no_file);
	tcase_add_test(tcase_load, test_load_valid_config);
	tcase_add_test(tcase_load, test_load_partial_config_uses_defaults);
	tcase_add_test(tcase_load, test_load_invalid_keybinding_returns_null);
	tcase_add_test(tcase_load, test_load_empty_keybindings_section);
	tcase_add_test(tcase_load, test_load_no_keybindings_section);
	suite_add_tcase(suite, tcase_load);

	TCase *tcase_defaults = tcase_create("defaults");
	tcase_add_test(tcase_defaults, test_get_default_keybindings);
	suite_add_tcase(suite, tcase_defaults);

	return suite;
}

int
main(void)
{
	Suite *suite = waymux_config_suite();
	SRunner *runner = srunner_create(suite);

	srunner_run_all(runner, CK_NORMAL);
	int failed = srunner_ntests_failed(runner);
	srunner_free(runner);

	return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
