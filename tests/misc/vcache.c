#include <stic.h>

#include <test-utils.h>

#include "../../src/ui/ui.h"
#include "../../src/utils/string_array.h"
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
