/*
 * WayMux: A Wayland multiplexer.
 *
 * Copyright (C) 2025 Ido Perlmuter
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200809L

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "desktop_entry.h"

/* Test: desktop_entry_manager_create and destroy */
START_TEST(test_manager_create_destroy)
{
	struct cg_desktop_entry_manager *manager = desktop_entry_manager_create();

	ck_assert_ptr_nonnull(manager);

	/* Entries list should be initialized (next and prev point to itself) */
	ck_assert_ptr_eq(manager->entries.next, &manager->entries);
	ck_assert_ptr_eq(manager->entries.prev, &manager->entries);

	desktop_entry_manager_destroy(manager);
}
END_TEST

/* Test: desktop_entry_manager_search with empty query */
START_TEST(test_manager_search_empty)
{
	struct cg_desktop_entry_manager *manager = desktop_entry_manager_create();

	/* Create test entries manually */
	struct cg_desktop_entry *entry1 = calloc(1, sizeof(struct cg_desktop_entry));
	entry1->name = strdup("Application One");
	entry1->exec = strdup("/usr/bin/app1");
	entry1->icon = NULL;
	entry1->categories = NULL;
	entry1->desktop_file = NULL;
	entry1->nodisplay = false;
	wl_list_insert(&manager->entries, &entry1->link);

	struct cg_desktop_entry *entry2 = calloc(1, sizeof(struct cg_desktop_entry));
	entry2->name = strdup("Application Two");
	entry2->exec = strdup("/usr/bin/app2");
	entry2->icon = NULL;
	entry2->categories = NULL;
	entry2->desktop_file = NULL;
	entry2->nodisplay = false;
	wl_list_insert(&manager->entries, &entry2->link);

	/* Search with empty query should return all non-NoDisplay entries */
	struct cg_desktop_entry *results[10];
	size_t count = desktop_entry_manager_search(manager, "", results, 10);

	ck_assert_uint_eq(count, 2);

	/* Cleanup - manager_destroy will free all entries */
	desktop_entry_manager_destroy(manager);
}
END_TEST

/* Test: desktop_entry_manager_search with query */
START_TEST(test_manager_search_query)
{
	struct cg_desktop_entry_manager *manager = desktop_entry_manager_create();

	/* Create test entries */
	struct cg_desktop_entry *entry1 = calloc(1, sizeof(struct cg_desktop_entry));
	entry1->name = strdup("Firefox");
	entry1->exec = strdup("/usr/bin/firefox");
	entry1->nodisplay = false;
	wl_list_insert(&manager->entries, &entry1->link);

	struct cg_desktop_entry *entry2 = calloc(1, sizeof(struct cg_desktop_entry));
	entry2->name = strdup("Chrome");
	entry2->exec = strdup("/usr/bin/chrome");
	entry2->nodisplay = false;
	wl_list_insert(&manager->entries, &entry2->link);

	/* Search for "fire" should match Firefox */
	struct cg_desktop_entry *results[10];
	size_t count = desktop_entry_manager_search(manager, "fire", results, 10);

	ck_assert_uint_eq(count, 1);
	ck_assert_str_eq(results[0]->name, "Firefox");

	/* Cleanup - manager_destroy will free all entries */
	desktop_entry_manager_destroy(manager);
}
END_TEST

/* Test: desktop_entry_manager_search filters NoDisplay */
START_TEST(test_manager_search_nodisplay)
{
	struct cg_desktop_entry_manager *manager = desktop_entry_manager_create();

	/* Create test entries - one visible, one hidden */
	struct cg_desktop_entry *entry1 = calloc(1, sizeof(struct cg_desktop_entry));
	entry1->name = strdup("Visible App");
	entry1->exec = strdup("/usr/bin/visible");
	entry1->nodisplay = false;
	wl_list_insert(&manager->entries, &entry1->link);

	struct cg_desktop_entry *entry2 = calloc(1, sizeof(struct cg_desktop_entry));
	entry2->name = strdup("Hidden App");
	entry2->exec = strdup("/usr/bin/hidden");
	entry2->nodisplay = true;
	wl_list_insert(&manager->entries, &entry2->link);

	/* Empty query should only return visible entries */
	struct cg_desktop_entry *results[10];
	size_t count = desktop_entry_manager_search(manager, "", results, 10);

	ck_assert_uint_eq(count, 1);
	ck_assert_str_eq(results[0]->name, "Visible App");

	/* Cleanup - manager_destroy will free all entries */
	desktop_entry_manager_destroy(manager);
}
END_TEST

/* Test: desktop_entry_manager_search with NULL query */
START_TEST(test_manager_search_null)
{
	struct cg_desktop_entry_manager *manager = desktop_entry_manager_create();

	/* Create test entry */
	struct cg_desktop_entry *entry1 = calloc(1, sizeof(struct cg_desktop_entry));
	entry1->name = strdup("Test App");
	entry1->exec = strdup("/usr/bin/test");
	entry1->nodisplay = false;
	wl_list_insert(&manager->entries, &entry1->link);

	/* NULL query should return all entries */
	struct cg_desktop_entry *results[10];
	size_t count = desktop_entry_manager_search(manager, NULL, results, 10);

	ck_assert_uint_eq(count, 1);

	/* Cleanup - manager_destroy will free all entries */
	desktop_entry_manager_destroy(manager);
}
END_TEST

/* Test: desktop_entry_destroy with NULL */
START_TEST(test_desktop_entry_destroy_null)
{
	/* Should not crash */
	desktop_entry_destroy(NULL);
}
END_TEST

/* Test: desktop_entry_manager_destroy with NULL */
START_TEST(test_manager_destroy_null)
{
	/* Should not crash */
	desktop_entry_manager_destroy(NULL);
}
END_TEST

/* Test: desktop_entry_manager_search with NULL manager */
START_TEST(test_manager_search_null_manager)
{
	struct cg_desktop_entry *results[10];

	/* Should not crash and return 0 */
	size_t count = desktop_entry_manager_search(NULL, "test", results, 10);

	ck_assert_uint_eq(count, 0);
}
END_TEST

/* Test: case insensitive search */
START_TEST(test_manager_search_case_insensitive)
{
	struct cg_desktop_entry_manager *manager = desktop_entry_manager_create();

	/* Create test entries with mixed case */
	struct cg_desktop_entry *entry1 = calloc(1, sizeof(struct cg_desktop_entry));
	entry1->name = strdup("FIREFOX");
	entry1->exec = strdup("/usr/bin/firefox");
	entry1->nodisplay = false;
	wl_list_insert(&manager->entries, &entry1->link);

	struct cg_desktop_entry *entry2 = calloc(1, sizeof(struct cg_desktop_entry));
	entry2->name = strdup("Chrome");
	entry2->exec = strdup("/usr/bin/chrome");
	entry2->nodisplay = false;
	wl_list_insert(&manager->entries, &entry2->link);

	/* Lowercase search should match uppercase entry */
	struct cg_desktop_entry *results[10];
	size_t count = desktop_entry_manager_search(manager, "fire", results, 10);

	ck_assert_uint_eq(count, 1);
	ck_assert_str_eq(results[0]->name, "FIREFOX");

	/* Cleanup - manager_destroy will free all entries */
	desktop_entry_manager_destroy(manager);
}
END_TEST

/* Main test runner */
int
main(void)
{
	int number_failed;

	Suite *s = suite_create("Desktop Entry");
	TCase *tc_core = tcase_create("Core");
	TCase *tc_search = tcase_create("Search");

	/* Core tests */
	tcase_add_test(tc_core, test_manager_create_destroy);
	tcase_add_test(tc_core, test_desktop_entry_destroy_null);
	tcase_add_test(tc_core, test_manager_destroy_null);
	tcase_add_test(tc_core, test_manager_search_null_manager);

	/* Search tests */
	tcase_add_test(tc_search, test_manager_search_empty);
	tcase_add_test(tc_search, test_manager_search_query);
	tcase_add_test(tc_search, test_manager_search_nodisplay);
	tcase_add_test(tc_search, test_manager_search_null);
	tcase_add_test(tc_search, test_manager_search_case_insensitive);

	suite_add_tcase(s, tc_core);
	suite_add_tcase(s, tc_search);

	SRunner *sr = srunner_create(s);
	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
