# WayMux Unit Tests

This directory contains unit tests for WayMux using the Check testing framework.

## Current Status

The test framework infrastructure is in place. However, some tests currently fail due to bugs in the implementation that they expose:

1. **desktop_entry_test.c**: Tests time out due to a bug in `desktop_entry_manager_search()` where `wl_list_insert()` creates circular references when moving nodes between lists
2. **tab_test.c**: Some tests fail due to the stub implementation not matching the actual behavior

These test failures are valuable - they expose real bugs that need fixing. The tests are structured to prevent crashes while documenting known issues.

## Prerequisites

To build and run the tests, you need the Check framework installed:

### Arch Linux
```bash
sudo pacman -S check
```

### Debian/Ubuntu
```bash
sudo apt-get install libcheck-dev
```

### Fedora/RHEL
```bash
sudo dnf install check-devel
```

### FreeBSD
```bash
pkg install check
```

## Building the Tests

Configure Meson with tests enabled:

```bash
meson setup build -Dtests=true
```

Then build as usual:

```bash
ninja -C build
```

## Running the Tests

Run all tests:

```bash
ninja -C build test
```

Run specific test suites:

```bash
./build/desktop_entry_test
./build/tab_test
```

## Test Coverage

### desktop_entry_test.c
Tests for the XDG desktop entry parsing module:
- Manager creation and destruction
- NULL handling for all functions
- Search functionality (currently exposes bugs in implementation)
- NoDisplay filtering (currently exposes bugs in implementation)
- Case-insensitive substring matching (currently exposes bugs in implementation)

**Known Issues**: The `desktop_entry_manager_search()` function has a bug where it uses `wl_list_insert()` to move nodes between lists, which can create circular references. The tests document this behavior for future fixes.

### tab_test.c
Tests for tab management:
- Tab counting
- Tab navigation (next/previous with wraparound)
- NULL pointer handling
- View-to-tab lookup

**Note**: Tab tests use a stub implementation (`tab_test_stubs.c`) to avoid linking the entire WayMux codebase. Only the pure data structure functions are tested.

## Adding New Tests

1. Create a new test file in this directory following the pattern `*_test.c`
2. Include the Check framework header: `#include <check.h>`
3. Create test cases using `START_TEST` / `END_TEST` macros
4. Add the test executable to `meson.build` in the root directory
5. Add the test to the test runner with `test()`

Example test structure:

```c
#include <check.h>
#include "module_under_test.h"

START_TEST(test_feature)
{
    // Setup
    int input = 5;

    // Execute
    int result = function_under_test(input);

    // Assert
    ck_assert_int_eq(result, 10);
}
END_TEST

int main(void) {
    Suite *s = suite_create("Module Name");
    TCase *tc = tcase_create("Core");

    tcase_add_test(tc, test_feature);

    suite_add_tcase(s, tc);

    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
```
