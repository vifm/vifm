#include <stic.h>

#include <sys/stat.h> /* chmod() */

#include <test-utils.h>

#include "../../src/ui/quickview.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/string_array.h"
#include "../../src/status.h"
#include "../../src/vcache.h"

static const char *error;

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
	vcache_reset(3);
}

TEARDOWN()
{
	conf_teardown();
}

TEST(missing_file_is_handled)
{
	strlist_t lines = vcache_lookup(SANDBOX_PATH "/no-file", NULL, VK_TEXTUAL,
			10, &error);
	assert_string_equal("Failed to read file's contents", error);
	assert_int_equal(0, lines.nitems);
}

TEST(unreadable_directory_file_is_handled, IF(not_windows))
{
	create_dir(SANDBOX_PATH "/dir");
	assert_success(chmod(SANDBOX_PATH "/dir", 0000));

	strlist_t lines = vcache_lookup(SANDBOX_PATH "/dir", NULL, VK_TEXTUAL, 10,
			&error);
	assert_string_equal("Failed to list directory's contents", error);
	assert_int_equal(0, lines.nitems);

	remove_dir(SANDBOX_PATH "/dir");
}

TEST(can_view_full_file)
{
	strlist_t lines = vcache_lookup(TEST_DATA_PATH "/read/two-lines", NULL,
			VK_TEXTUAL, 10, &error);
	assert_string_equal(NULL, error);
	assert_int_equal(2, lines.nitems);
	assert_string_equal("1st line", lines.items[0]);
	assert_string_equal("2nd line", lines.items[1]);
}

TEST(can_view_partial_file)
{
	strlist_t lines = vcache_lookup(TEST_DATA_PATH "/read/two-lines", NULL,
			VK_TEXTUAL, 1, &error);
	assert_string_equal(NULL, error);
	assert_int_equal(1, lines.nitems);
	assert_string_equal("1st line", lines.items[0]);
}

TEST(can_view_directory)
{
	strlist_t lines = vcache_lookup(TEST_DATA_PATH "/rename", NULL, VK_TEXTUAL,
			10, &error);
	assert_string_equal(NULL, error);
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
	strlist_t lines = vcache_lookup(TEST_DATA_PATH "/read/", "echo text",
			VK_TEXTUAL, 1, &error);
	assert_string_equal(NULL, error);
	assert_int_equal(1, lines.nitems);
	assert_string_equal("text", lines.items[0]);
}

TEST(single_file_data_is_cached)
{
	strlist_t lines1, lines2;

	/* Two lines are cached. */
	lines1 = vcache_lookup(TEST_DATA_PATH "/read/dos-line-endings", NULL,
			VK_TEXTUAL, 2, &error);
	assert_string_equal(NULL, error);
	assert_int_equal(2, lines1.nitems);
	assert_string_equal("first line", lines1.items[0]);
	assert_string_equal("second line", lines1.items[1]);

	/* Previously cached data is returned. */
	lines2 = vcache_lookup(TEST_DATA_PATH "/read/dos-line-endings", NULL,
			VK_TEXTUAL, 1, &error);
	assert_string_equal(NULL, error);
	assert_int_equal(2, lines2.nitems);
	assert_true(lines1.items[0] == lines2.items[0]);
	assert_true(lines1.items[1] == lines2.items[1]);
}

TEST(multiple_files_data_is_cached)
{
	strlist_t f1lines1, f1lines2;
	strlist_t f2lines1, f2lines2;

	/* Two lines are cached. */
	f1lines1 = vcache_lookup(TEST_DATA_PATH "/read/dos-line-endings", NULL,
			VK_TEXTUAL, 2, &error);
	assert_string_equal(NULL, error);
	assert_int_equal(2, f1lines1.nitems);
	assert_string_equal("first line", f1lines1.items[0]);
	assert_string_equal("second line", f1lines1.items[1]);

	/* Two lines are cached. */
	f2lines1 = vcache_lookup(TEST_DATA_PATH "/read/dos-line-endings", NULL,
			VK_TEXTUAL, 2, &error);
	assert_string_equal(NULL, error);
	assert_int_equal(2, f2lines1.nitems);
	assert_string_equal("first line", f2lines1.items[0]);
	assert_string_equal("second line", f2lines1.items[1]);

	/* Previously cached data is returned. */
	f1lines2 = vcache_lookup(TEST_DATA_PATH "/read/dos-line-endings", NULL,
			VK_TEXTUAL, 1, &error);
	assert_string_equal(NULL, error);
	assert_int_equal(2, f1lines2.nitems);
	assert_true(f1lines1.items[0] == f1lines2.items[0]);
	assert_true(f1lines1.items[1] == f1lines2.items[1]);

	/* Previously cached data is returned. */
	f2lines2 = vcache_lookup(TEST_DATA_PATH "/read/dos-line-endings", NULL,
			VK_TEXTUAL, 1, &error);
	assert_string_equal(NULL, error);
	assert_int_equal(2, f2lines2.nitems);
	assert_true(f2lines1.items[0] == f2lines2.items[0]);
	assert_true(f2lines1.items[1] == f2lines2.items[1]);
}

TEST(failure_to_allocate_cache_entry_is_handled)
{
	vcache_reset(0);
	strlist_t lines = vcache_lookup(TEST_DATA_PATH "/read/dos-line-endings", NULL,
			VK_TEXTUAL, 2, &error);
	assert_string_equal("Failed to allocate cache entry", error);
	assert_int_equal(0, lines.nitems);
}

TEST(cache_entries_are_reused)
{
	/* Two lines are cached. */
	strlist_t lines = vcache_lookup(TEST_DATA_PATH "/read/dos-line-endings", NULL,
			VK_TEXTUAL, 2, &error);
	assert_string_equal(NULL, error);
	assert_int_equal(2, lines.nitems);
	assert_string_equal("first line", lines.items[0]);
	assert_string_equal("second line", lines.items[1]);

	/* Push cached data out of the cache. */
	lines = vcache_lookup(TEST_DATA_PATH "/read/dos-line-endings", "echo a",
			VK_TEXTUAL, 2, &error);
	assert_string_equal(NULL, error);
	lines = vcache_lookup(TEST_DATA_PATH "/read/dos-line-endings", "echo b",
			VK_TEXTUAL, 2, &error);
	assert_string_equal(NULL, error);
	lines = vcache_lookup(TEST_DATA_PATH "/read/dos-line-endings", "echo c",
			VK_TEXTUAL, 2, &error);
	assert_string_equal(NULL, error);

	/* Previously cached data is not returned. */
	lines = vcache_lookup(TEST_DATA_PATH "/read/dos-line-endings", NULL,
			VK_TEXTUAL, 1, &error);
	assert_string_equal(NULL, error);
	assert_int_equal(1, lines.nitems);
	assert_string_equal("first line", lines.items[0]);
}

TEST(viewers_are_cached_independently)
{
	strlist_t lines1 = vcache_lookup(TEST_DATA_PATH "/read/two-lines", "echo aaa",
			VK_TEXTUAL, 10, &error);
	assert_string_equal(NULL, error);
	assert_int_equal(1, lines1.nitems);
	assert_string_equal("aaa", lines1.items[0]);

	strlist_t lines2 = vcache_lookup(TEST_DATA_PATH "/read/two-lines", "echo bbb",
			VK_TEXTUAL, 10, &error);
	assert_string_equal(NULL, error);
	assert_int_equal(1, lines2.nitems);
	assert_string_equal("bbb", lines2.items[0]);
}

TEST(file_modification_is_detected)
{
	make_file(SANDBOX_PATH "/file", "old line");

	strlist_t lines = vcache_lookup(SANDBOX_PATH "/file", NULL, VK_TEXTUAL, 10,
			&error);
	assert_string_equal(NULL, error);
	assert_int_equal(1, lines.nitems);
	assert_string_equal("old line", lines.items[0]);

	make_file(SANDBOX_PATH "/file", "new line");
	reset_timestamp(SANDBOX_PATH "/file");

	lines = vcache_lookup(SANDBOX_PATH "/file", NULL, VK_TEXTUAL, 10, &error);
	assert_string_equal(NULL, error);
	assert_int_equal(1, lines.nitems);
	assert_string_equal("new line", lines.items[0]);

	remove_file(SANDBOX_PATH "/file");
}

TEST(graphics_is_not_cached)
{
	preview_area_t parea = { .view = curr_view };
	curr_stats.preview_hint = &parea;

	strlist_t lines1 = vcache_lookup(TEST_DATA_PATH "/read/two-lines",
			"echo this", VK_GRAPHICAL, 10, &error);
	assert_string_equal(NULL, error);
	assert_int_equal(1, lines1.nitems);
	assert_string_equal("this", lines1.items[0]);
	lines1.items = copy_string_array(lines1.items, lines1.nitems);

	strlist_t lines2 = vcache_lookup(TEST_DATA_PATH "/read/two-lines",
			"echo this", VK_GRAPHICAL, 10, &error);
	assert_string_equal(NULL, error);
	assert_int_equal(1, lines2.nitems);
	assert_string_equal("this", lines1.items[0]);
	assert_true(lines1.items != lines2.items);
	assert_true(lines1.items[0] != lines2.items[0]);

	free_string_array(lines1.items, lines1.nitems);
	curr_stats.preview_hint = NULL;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
