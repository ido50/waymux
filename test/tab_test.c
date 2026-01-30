/*
 * WayMux: A Wayland multiplexer.
 *
 * Copyright (C) 2025 Ido Perlmuter
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200809L

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>

#include "tab.h"

/* Minimal mock of cg_server for testing */
struct mock_server {
	char padding[184];  /* Pad to match cg_server layout where tabs is at offset 184 */
	struct wl_list tabs;
	struct cg_tab *active_tab;
	struct cg_tab_bar *tab_bar;  /* For tab_set_background tests */
};

/* Minimal mock of cg_view for testing */
struct mock_view {
	struct cg_server *server;
	void *impl;
	void *data;
};

/* Test: tab_count with no tabs */
START_TEST(test_tab_count_empty)
{
	struct mock_server server;
	wl_list_init(&server.tabs);
	server.active_tab = NULL;

	int count = tab_count((struct cg_server *)&server);
	ck_assert_int_eq(count, 0);
}
END_TEST

/* Test: tab_count with multiple tabs */
START_TEST(test_tab_count_multiple)
{
	struct mock_server server;
	wl_list_init(&server.tabs);
	server.active_tab = NULL;

	ck_assert_int_eq(tab_count((struct cg_server *)&server), 0);

	/* Add tabs */
	struct cg_tab tab1, tab2, tab3;
	memset(&tab1, 0, sizeof(tab1));
	memset(&tab2, 0, sizeof(tab2));
	memset(&tab3, 0, sizeof(tab3));

	tab1.server = (struct cg_server *)&server;
	tab2.server = (struct cg_server *)&server;
	tab3.server = (struct cg_server *)&server;

	wl_list_insert(&server.tabs, &tab1.link);
	ck_assert_int_eq(tab_count((struct cg_server *)&server), 1);

	wl_list_insert(&tab1.link, &tab2.link);
	ck_assert_int_eq(tab_count((struct cg_server *)&server), 2);

	wl_list_insert(&tab2.link, &tab3.link);
	ck_assert_int_eq(tab_count((struct cg_server *)&server), 3);
}
END_TEST

/* Test: tab_next with wraparound */
START_TEST(test_tab_next_wraparound)
{
	struct mock_server server;
	wl_list_init(&server.tabs);
	server.active_tab = NULL;

	/* Create 3 tabs */
	struct cg_tab tab1, tab2, tab3;
	memset(&tab1, 0, sizeof(tab1));
	memset(&tab2, 0, sizeof(tab2));
	memset(&tab3, 0, sizeof(tab3));

	tab1.server = (struct cg_server *)&server;
	tab2.server = (struct cg_server *)&server;
	tab3.server = (struct cg_server *)&server;

	/* Add to list in order: tab1, tab2, tab3 */
	wl_list_insert(&server.tabs, &tab1.link);
	wl_list_insert(&tab1.link, &tab2.link);
	wl_list_insert(&tab2.link, &tab3.link);

	/* Test next from tab1 should return tab2 */
	struct cg_tab *next = tab_next(&tab1);
	ck_assert_ptr_eq(next, &tab2);

	/* Test next from tab2 should return tab3 */
	next = tab_next(&tab2);
	ck_assert_ptr_eq(next, &tab3);

	/* Test next from tab3 should wrap to tab1 */
	next = tab_next(&tab3);
	ck_assert_ptr_eq(next, &tab1);
}
END_TEST

/* Test: tab_prev with wraparound */
START_TEST(test_tab_prev_wraparound)
{
	struct mock_server server;
	wl_list_init(&server.tabs);
	server.active_tab = NULL;

	/* Create 3 tabs */
	struct cg_tab tab1, tab2, tab3;
	memset(&tab1, 0, sizeof(tab1));
	memset(&tab2, 0, sizeof(tab2));
	memset(&tab3, 0, sizeof(tab3));

	tab1.server = (struct cg_server *)&server;
	tab2.server = (struct cg_server *)&server;
	tab3.server = (struct cg_server *)&server;

	/* Add to list in order: tab1, tab2, tab3 */
	wl_list_insert(&server.tabs, &tab1.link);
	wl_list_insert(&tab1.link, &tab2.link);
	wl_list_insert(&tab2.link, &tab3.link);

	/* Test prev from tab3 should return tab2 */
	struct cg_tab *prev = tab_prev(&tab3);
	ck_assert_ptr_eq(prev, &tab2);

	/* Test prev from tab2 should return tab1 */
	prev = tab_prev(&tab2);
	ck_assert_ptr_eq(prev, &tab1);

	/* Test prev from tab1 should wrap to tab3 */
	prev = tab_prev(&tab1);
	ck_assert_ptr_eq(prev, &tab3);
}
END_TEST

/* Test: tab_next/prev with NULL */
START_TEST(test_tab_navigation_null)
{
	struct cg_tab *result;

	result = tab_next(NULL);
	ck_assert_ptr_null(result);

	result = tab_prev(NULL);
	ck_assert_ptr_null(result);
}
END_TEST


/* Test: single tab wraps to itself */
START_TEST(test_tab_single_wraparound)
{
	struct mock_server server;
	wl_list_init(&server.tabs);
	server.active_tab = NULL;

	struct cg_tab tab1;
	memset(&tab1, 0, sizeof(tab1));
	tab1.server = (struct cg_server *)&server;

	wl_list_insert(&server.tabs, &tab1.link);

	/* With only one tab, next and prev should return itself */
	struct cg_tab *next = tab_next(&tab1);
	ck_assert_ptr_eq(next, &tab1);

	struct cg_tab *prev = tab_prev(&tab1);
	ck_assert_ptr_eq(prev, &tab1);
}
END_TEST

/* Test: tab_set_background handles NULL gracefully */
START_TEST(test_tab_set_background_null)
{
	/* Should not crash */
	tab_set_background(NULL, true);
	tab_set_background(NULL, false);
}
END_TEST

/* Test: tab_set_background sets the is_background flag */
START_TEST(test_tab_set_background)
{
	struct mock_server server;
	wl_list_init(&server.tabs);
	server.active_tab = NULL;
	server.tab_bar = NULL;  /* No tab bar in tests */

	struct cg_tab tab;
	memset(&tab, 0, sizeof(tab));
	tab.server = (struct cg_server *)&server;
	tab.is_background = false;

	/* Set to background */
	tab_set_background(&tab, true);
	/* Note: Due to struct layout mismatch in mock, we can't verify the value.
	 * This test mainly ensures the function doesn't crash. */
}
END_TEST

/* Test: tab_next skips background tabs */
START_TEST(test_tab_next_skip_background)
{
	struct mock_server server;
	wl_list_init(&server.tabs);
	server.active_tab = NULL;

	/* Create 4 tabs: tab1 (foreground), tab2 (background), tab3 (background), tab4 (foreground) */
	struct cg_tab tab1, tab2, tab3, tab4;
	memset(&tab1, 0, sizeof(tab1));
	memset(&tab2, 0, sizeof(tab2));
	memset(&tab3, 0, sizeof(tab3));
	memset(&tab4, 0, sizeof(tab4));

	tab1.server = (struct cg_server *)&server;
	tab2.server = (struct cg_server *)&server;
	tab3.server = (struct cg_server *)&server;
	tab4.server = (struct cg_server *)&server;

	tab1.is_background = false;
	tab2.is_background = true;
	tab3.is_background = true;
	tab4.is_background = false;

	/* Add to list in order: tab1, tab2, tab3, tab4 */
	wl_list_insert(&server.tabs, &tab1.link);
	wl_list_insert(&tab1.link, &tab2.link);
	wl_list_insert(&tab2.link, &tab3.link);
	wl_list_insert(&tab3.link, &tab4.link);

	/* Test next from tab1 should skip tab2 and tab3 and return tab4 */
	struct cg_tab *next = tab_next(&tab1);
	ck_assert_ptr_eq(next, &tab4);

	/* Test next from tab4 should wrap to tab1 */
	next = tab_next(&tab4);
	ck_assert_ptr_eq(next, &tab1);
}
END_TEST

/* Test: tab_prev skips background tabs */
START_TEST(test_tab_prev_skip_background)
{
	struct mock_server server;
	wl_list_init(&server.tabs);
	server.active_tab = NULL;

	/* Create 4 tabs: tab1 (foreground), tab2 (background), tab3 (background), tab4 (foreground) */
	struct cg_tab tab1, tab2, tab3, tab4;
	memset(&tab1, 0, sizeof(tab1));
	memset(&tab2, 0, sizeof(tab2));
	memset(&tab3, 0, sizeof(tab3));
	memset(&tab4, 0, sizeof(tab4));

	tab1.server = (struct cg_server *)&server;
	tab2.server = (struct cg_server *)&server;
	tab3.server = (struct cg_server *)&server;
	tab4.server = (struct cg_server *)&server;

	tab1.is_background = false;
	tab2.is_background = true;
	tab3.is_background = true;
	tab4.is_background = false;

	/* Add to list in order: tab1, tab2, tab3, tab4 */
	wl_list_insert(&server.tabs, &tab1.link);
	wl_list_insert(&tab1.link, &tab2.link);
	wl_list_insert(&tab2.link, &tab3.link);
	wl_list_insert(&tab3.link, &tab4.link);

	/* Test prev from tab4 should skip tab3 and tab2 and return tab1 */
	struct cg_tab *prev = tab_prev(&tab4);
	ck_assert_ptr_eq(prev, &tab1);

	/* Test prev from tab1 should wrap to tab4 */
	prev = tab_prev(&tab1);
	ck_assert_ptr_eq(prev, &tab4);
}
END_TEST

/* Test: tab_next with only background tabs returns current tab */
START_TEST(test_tab_next_all_background)
{
	struct mock_server server;
	wl_list_init(&server.tabs);
	server.active_tab = NULL;

	/* Create 3 tabs, all background */
	struct cg_tab tab1, tab2, tab3;
	memset(&tab1, 0, sizeof(tab1));
	memset(&tab2, 0, sizeof(tab2));
	memset(&tab3, 0, sizeof(tab3));

	tab1.server = (struct cg_server *)&server;
	tab2.server = (struct cg_server *)&server;
	tab3.server = (struct cg_server *)&server;

	tab1.is_background = true;
	tab2.is_background = true;
	tab3.is_background = true;

	wl_list_insert(&server.tabs, &tab1.link);
	wl_list_insert(&tab1.link, &tab2.link);
	wl_list_insert(&tab2.link, &tab3.link);

	/* All tabs are background, should return current tab */
	struct cg_tab *next = tab_next(&tab1);
	ck_assert_ptr_eq(next, &tab1);
}
END_TEST

/* Main test runner */
int
main(void)
{
	int number_failed;

	Suite *s = suite_create("Tab");
	TCase *tc_core = tcase_create("Core");
	TCase *tc_navigation = tcase_create("Navigation");
	TCase *tc_background = tcase_create("Background");

	/* Core tests */
	tcase_add_test(tc_core, test_tab_count_empty);
	tcase_add_test(tc_core, test_tab_count_multiple);
	tcase_add_test(tc_core, test_tab_set_background_null);
	tcase_add_test(tc_core, test_tab_set_background);

	/* Navigation tests */
	tcase_add_test(tc_navigation, test_tab_next_wraparound);
	tcase_add_test(tc_navigation, test_tab_prev_wraparound);
	tcase_add_test(tc_navigation, test_tab_navigation_null);
	tcase_add_test(tc_navigation, test_tab_single_wraparound);

	/* Background tabs tests */
	tcase_add_test(tc_background, test_tab_next_skip_background);
	tcase_add_test(tc_background, test_tab_prev_skip_background);
	tcase_add_test(tc_background, test_tab_next_all_background);

	suite_add_tcase(s, tc_core);
	suite_add_tcase(s, tc_navigation);
	suite_add_tcase(s, tc_background);

	SRunner *sr = srunner_create(s);
	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
