#include <stic.h>

#include <test-utils.h>

#include "../../src/ui/quickview.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/string_array.h"
#include "../../src/status.h"
#include "../../src/vcache.h"

SETUP_ONCE()
{
	curr_view = &lwin;
	other_view = &rwin;
}

TEARDOWN_ONCE()
{
	curr_view = NULL;
	other_view = NULL;
}

SETUP()
{
	conf_setup();
}

TEARDOWN()
{
	conf_teardown();
	vcache_reset();
}

TEST(missing_file_is_handled)
{
	strlist_t lines = vcache_lookup(SANDBOX_PATH "/no-file", NULL, 10);
	assert_int_equal(1, lines.nitems);
	assert_string_equal("Vifm: previewing has failed", lines.items[0]);
}

TEST(can_view_full_file)
{
	strlist_t lines = vcache_lookup(TEST_DATA_PATH "/read/two-lines", NULL, 10);
	assert_int_equal(2, lines.nitems);
	assert_string_equal("1st line", lines.items[0]);
	assert_string_equal("2nd line", lines.items[1]);
}

TEST(can_view_partial_file)
{
	strlist_t lines = vcache_lookup(TEST_DATA_PATH "/read/two-lines", NULL, 1);
	assert_int_equal(1, lines.nitems);
	assert_string_equal("1st line", lines.items[0]);
}

TEST(can_view_directory)
{
	strlist_t lines = vcache_lookup(TEST_DATA_PATH "/rename", NULL, 10);
	assert_int_equal(6, lines.nitems);
	assert_string_equal("rename/", lines.items[0]);
	assert_string_equal("|-- a", lines.items[1]);
	assert_string_equal("|-- aa", lines.items[2]);
	assert_string_equal("`-- aaa", lines.items[3]);
	assert_string_equal("", lines.items[4]);
	assert_string_equal("0 directories, 3 files", lines.items[5]);
}

TEST(can_use_custom_viewer)
{
	strlist_t lines = vcache_lookup(TEST_DATA_PATH "/read/", "echo h%%i", 1);
	assert_int_equal(1, lines.nitems);
	assert_string_equal("h%i", lines.items[0]);
}

TEST(single_file_data_is_cached)
{
	strlist_t lines1, lines2;

	/* Two lines are cached. */
	lines1 = vcache_lookup(TEST_DATA_PATH "/read/dos-line-endings", NULL, 2);
	assert_int_equal(2, lines1.nitems);
	assert_string_equal("first line", lines1.items[0]);
	assert_string_equal("second line", lines1.items[1]);

	/* Previously cached data is returned. */
	lines2 = vcache_lookup(TEST_DATA_PATH "/read/dos-line-endings", NULL, 1);
	assert_int_equal(2, lines2.nitems);
	assert_true(lines1.items[0] == lines2.items[0]);
	assert_true(lines1.items[1] == lines2.items[1]);
}

TEST(viewers_are_cached_independently)
{
	strlist_t lines1 = vcache_lookup(TEST_DATA_PATH "/read/two-lines", "echo %%a",
			10);
	assert_int_equal(1, lines1.nitems);
	assert_string_equal("%a", lines1.items[0]);

	strlist_t lines2 = vcache_lookup(TEST_DATA_PATH "/read/two-lines", "echo %%b",
			10);
	assert_int_equal(1, lines2.nitems);
	assert_string_equal("%b", lines2.items[0]);
}

TEST(file_modification_is_detected)
{
	make_file(SANDBOX_PATH "/file", "old line");

	strlist_t lines = vcache_lookup(SANDBOX_PATH "/file", NULL, 10);
	assert_int_equal(1, lines.nitems);
	assert_string_equal("old line", lines.items[0]);

	make_file(SANDBOX_PATH "/file", "new line");
	reset_timestamp(SANDBOX_PATH "/file");

	lines = vcache_lookup(SANDBOX_PATH "/file", NULL, 10);
	assert_int_equal(1, lines.nitems);
	assert_string_equal("new line", lines.items[0]);

	remove_file(SANDBOX_PATH "/file");
}

TEST(graphics_is_not_cached)
{
	preview_area_t parea = { .view = curr_view };
	curr_stats.preview_hint = &parea;

	strlist_t lines1 = vcache_lookup(TEST_DATA_PATH "/read/two-lines",
			"echo %px%py", 10);
	assert_int_equal(1, lines1.nitems);
	assert_string_equal("-1-1", lines1.items[0]);
	lines1.items = copy_string_array(lines1.items, lines1.nitems);

	strlist_t lines2 = vcache_lookup(TEST_DATA_PATH "/read/two-lines",
			"echo %px%py", 10);
	assert_int_equal(1, lines2.nitems);
	assert_string_equal("-1-1", lines1.items[0]);
	assert_true(lines1.items != lines2.items);
	assert_true(lines1.items[0] != lines2.items[0]);

	free_string_array(lines1.items, lines1.nitems);
	curr_stats.preview_hint = NULL;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
