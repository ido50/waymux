/*
 * WayMux: A Wayland multiplexer.
 *
 * Copyright (C) 2025 Ido Perlmuter
 *
 * See the LICENSE file accompanying this file.
 */

#include <check.h>
#include <stdlib.h>
#include <wlr/types/wlr_keyboard.h>
#include <xkbcommon/xkbcommon.h>
#include "../keybinding.h"

START_TEST(test_parse_simple)
{
	struct keybinding binding;

	/* Simple Super+J */
	ck_assert_msg(keybinding_parse("Super+J", &binding), "Failed to parse Super+J");
	ck_assert_uint_eq(binding.modifiers, WLR_MODIFIER_LOGO);
	ck_assert_uint_eq(binding.keysym, XKB_KEY_j);

	/* Simple Ctrl+K */
	ck_assert_msg(keybinding_parse("Ctrl+K", &binding), "Failed to parse Ctrl+K");
	ck_assert_uint_eq(binding.modifiers, WLR_MODIFIER_CTRL);
	ck_assert_uint_eq(binding.keysym, XKB_KEY_k);

	/* Simple Alt+D */
	ck_assert_msg(keybinding_parse("Alt+D", &binding), "Failed to parse Alt+D");
	ck_assert_uint_eq(binding.modifiers, WLR_MODIFIER_ALT);
	ck_assert_uint_eq(binding.keysym, XKB_KEY_d);

	/* Simple Shift+N */
	ck_assert_msg(keybinding_parse("Shift+N", &binding), "Failed to parse Shift+N");
	ck_assert_uint_eq(binding.modifiers, WLR_MODIFIER_SHIFT);
	ck_assert_uint_eq(binding.keysym, XKB_KEY_n);
}
END_TEST

START_TEST(test_parse_multiple_modifiers)
{
	struct keybinding binding;

	/* Super+Shift+B */
	ck_assert_msg(keybinding_parse("Super+Shift+B", &binding), "Failed to parse Super+Shift+B");
	ck_assert_uint_eq(binding.modifiers, WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT);
	ck_assert_uint_eq(binding.keysym, XKB_KEY_b);

	/* Ctrl+Alt+Q */
	ck_assert_msg(keybinding_parse("Ctrl+Alt+Q", &binding), "Failed to parse Ctrl+Alt+Q");
	ck_assert_uint_eq(binding.modifiers, WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT);
	ck_assert_uint_eq(binding.keysym, XKB_KEY_q);

	/* Ctrl+Shift+Alt+X */
	ck_assert_msg(keybinding_parse("Ctrl+Shift+Alt+X", &binding), "Failed to parse Ctrl+Shift+Alt+X");
	ck_assert_uint_eq(binding.modifiers, WLR_MODIFIER_CTRL | WLR_MODIFIER_SHIFT | WLR_MODIFIER_ALT);
	ck_assert_uint_eq(binding.keysym, XKB_KEY_x);
}
END_TEST

START_TEST(test_parse_case_insensitive)
{
	struct keybinding binding1, binding2, binding3;

	/* Modifiers are case-insensitive */
	ck_assert(keybinding_parse("Super+j", &binding1));
	ck_assert(keybinding_parse("super+J", &binding2));
	ck_assert(keybinding_parse("SUPER+J", &binding3));

	ck_assert_uint_eq(binding1.modifiers, binding2.modifiers);
	ck_assert_uint_eq(binding2.modifiers, binding3.modifiers);
	ck_assert_uint_eq(binding1.keysym, binding2.keysym);
	ck_assert_uint_eq(binding2.keysym, binding3.keysym);

	/* Keys are case-insensitive too */
	ck_assert_uint_eq(binding1.keysym, XKB_KEY_j);
}
END_TEST

START_TEST(test_parse_invalid)
{
	struct keybinding binding;

	/* Empty string */
	ck_assert_msg(!keybinding_parse("", &binding), "Should not parse empty string");

	/* Only modifiers, no key */
	ck_assert_msg(!keybinding_parse("Super+Shift", &binding), "Should not parse without key");

	/* Invalid key name */
	ck_assert_msg(!keybinding_parse("Super+InvalidKey123", &binding), "Should not parse invalid key");

	/* NULL input */
	ck_assert_msg(!keybinding_parse(NULL, &binding), "Should not parse NULL");
	ck_assert_msg(!keybinding_parse("Super+J", NULL), "Should not handle NULL output");
}
END_TEST

START_TEST(test_match)
{
	struct keybinding binding = {WLR_MODIFIER_LOGO, XKB_KEY_k};

	/* Exact match */
	ck_assert(keybinding_match(&binding, WLR_MODIFIER_LOGO, XKB_KEY_k));

	/* Wrong modifiers */
	ck_assert(!keybinding_match(&binding, WLR_MODIFIER_CTRL, XKB_KEY_k));
	ck_assert(!keybinding_match(&binding, WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_k));

	/* Wrong key */
	ck_assert(!keybinding_match(&binding, WLR_MODIFIER_LOGO, XKB_KEY_j));

	/* NULL binding */
	ck_assert(!keybinding_match(NULL, WLR_MODIFIER_LOGO, XKB_KEY_k));
}
END_TEST

START_TEST(test_parse_function_keys)
{
	struct keybinding binding;

	/* Function keys */
	ck_assert(keybinding_parse("Super+F1", &binding));
	ck_assert_uint_eq(binding.keysym, XKB_KEY_F1);

	ck_assert(keybinding_parse("Ctrl+F12", &binding));
	ck_assert_uint_eq(binding.keysym, XKB_KEY_F12);

	/* Special keys */
	ck_assert(keybinding_parse("Super+Escape", &binding));
	ck_assert_uint_eq(binding.keysym, XKB_KEY_Escape);

	ck_assert(keybinding_parse("Super+Return", &binding));
	ck_assert_uint_eq(binding.keysym, XKB_KEY_Return);
}
END_TEST

Suite *
keybinding_suite(void)
{
	Suite *suite = suite_create("keybinding");

	TCase *tcase_parse = tcase_create("parse");
	tcase_add_test(tcase_parse, test_parse_simple);
	tcase_add_test(tcase_parse, test_parse_multiple_modifiers);
	tcase_add_test(tcase_parse, test_parse_case_insensitive);
	tcase_add_test(tcase_parse, test_parse_invalid);
	tcase_add_test(tcase_parse, test_parse_function_keys);
	suite_add_tcase(suite, tcase_parse);

	TCase *tcase_match = tcase_create("match");
	tcase_add_test(tcase_match, test_match);
	suite_add_tcase(suite, tcase_match);

	return suite;
}

int
main(void)
{
	Suite *suite = keybinding_suite();
	SRunner *runner = srunner_create(suite);

	srunner_run_all(runner, CK_NORMAL);
	int failed = srunner_ntests_failed(runner);
	srunner_free(runner);

	return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
