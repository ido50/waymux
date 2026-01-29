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
	char padding[176];  /* Pad to match cg_server layout where tabs is at offset 176 */
	struct wl_list tabs;
	struct cg_tab *active_tab;
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

/* Test: tab_from_view with NULL */
START_TEST(test_tab_from_view_null)
{
	struct cg_tab *result;

	result = tab_from_view(NULL);
	ck_assert_ptr_null(result);

	/* Test with view that has no server */
	struct mock_view view;
	memset(&view, 0, sizeof(view));
	view.server = NULL;

	result = tab_from_view((struct cg_view *)&view);
	ck_assert_ptr_null(result);
}
END_TEST

/* Test: tab_from_view with matching view */
START_TEST(test_tab_from_view_found)
{
	struct mock_server server;
	wl_list_init(&server.tabs);
	server.active_tab = NULL;

	/* Create view and tabs */
	struct mock_view view;
	memset(&view, 0, sizeof(view));
	view.server = (struct cg_server *)&server;

	struct cg_tab tab1, tab2;
	memset(&tab1, 0, sizeof(tab1));
	memset(&tab2, 0, sizeof(tab2));

	tab1.server = (struct cg_server *)&server;
	tab2.server = (struct cg_server *)&server;

	tab1.view = (struct cg_view *)&view;
	tab2.view = NULL;

	wl_list_insert(&server.tabs, &tab1.link);
	wl_list_insert(&tab1.link, &tab2.link);

	/* Should find tab1 */
	struct cg_tab *result = tab_from_view((struct cg_view *)&view);
	ck_assert_ptr_eq(result, &tab1);
}
END_TEST

/* Test: tab_from_view with no matching view */
START_TEST(test_tab_from_view_not_found)
{
	struct mock_server server;
	wl_list_init(&server.tabs);
	server.active_tab = NULL;

	/* Create view and tabs */
	struct mock_view view;
	memset(&view, 0, sizeof(view));
	view.server = (struct cg_server *)&server;

	struct cg_tab tab1;
	memset(&tab1, 0, sizeof(tab1));

	tab1.server = (struct cg_server *)&server;
	tab1.view = NULL;

	wl_list_insert(&server.tabs, &tab1.link);

	/* Should not find anything */
	struct cg_tab *result = tab_from_view((struct cg_view *)&view);
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

/* Main test runner */
int
main(void)
{
	int number_failed;

	Suite *s = suite_create("Tab");
	TCase *tc_core = tcase_create("Core");
	TCase *tc_navigation = tcase_create("Navigation");

	/* Core tests */
	tcase_add_test(tc_core, test_tab_count_empty);
	tcase_add_test(tc_core, test_tab_count_multiple);
	tcase_add_test(tc_core, test_tab_from_view_null);
	tcase_add_test(tc_core, test_tab_from_view_found);
	tcase_add_test(tc_core, test_tab_from_view_not_found);

	/* Navigation tests */
	tcase_add_test(tc_navigation, test_tab_next_wraparound);
	tcase_add_test(tc_navigation, test_tab_prev_wraparound);
	tcase_add_test(tc_navigation, test_tab_navigation_null);
	tcase_add_test(tc_navigation, test_tab_single_wraparound);

	suite_add_tcase(s, tc_core);
	suite_add_tcase(s, tc_navigation);

	SRunner *sr = srunner_create(s);
	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
