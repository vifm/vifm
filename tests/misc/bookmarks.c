#include "seatest.h"

#include <stddef.h> /* NULL */
#include <string.h> /* strlen() */

#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/bookmarks.h"

static void
test_unexistant_bookmark(void)
{
	assert_int_equal(1, goto_bookmark(&lwin, 'b'));
}

static void
test_all_valid_bookmarks_can_be_queried(void)
{
	const int bookmark_count = strlen(valid_bookmarks);
	int i;
	for(i = 0; i < bookmark_count; ++i)
	{
		assert_true(get_bookmark(i) != NULL);
	}
}

static void
test_regular_bmarks_are_global(void)
{
	const bookmark_t *bmark;

	curr_view = &lwin;
	set_user_bookmark('a', "lpath", "lfile");

	curr_view = &rwin;
	set_user_bookmark('a', "rpath", "rfile");

	curr_view = &lwin;
	bmark = get_bmark_by_name('a');
	assert_string_equal("rpath", bmark->directory);
	assert_string_equal("rfile", bmark->file);

	curr_view = &rwin;
	bmark = get_bmark_by_name('a');
	assert_string_equal("rpath", bmark->directory);
	assert_string_equal("rfile", bmark->file);
}

static void
test_sel_bmarks_are_local(void)
{
	const bookmark_t *bmark;

	curr_view = &lwin;
	set_spec_bookmark('<', "lpath", "lfile<");
	set_spec_bookmark('>', "lpath", "lfile>");

	curr_view = &rwin;
	set_spec_bookmark('<', "rpath", "rfile<");
	set_spec_bookmark('>', "rpath", "rfile>");

	curr_view = &lwin;
	bmark = get_bmark_by_name('<');
	assert_string_equal("lpath", bmark->directory);
	assert_string_equal("lfile<", bmark->file);
	bmark = get_bmark_by_name('>');
	assert_string_equal("lpath", bmark->directory);
	assert_string_equal("lfile>", bmark->file);

	curr_view = &rwin;
	bmark = get_bmark_by_name('<');
	assert_string_equal("rpath", bmark->directory);
	assert_string_equal("rfile<", bmark->file);
	bmark = get_bmark_by_name('>');
	assert_string_equal("rpath", bmark->directory);
	assert_string_equal("rfile>", bmark->file);
}

void
bookmarks_tests(void)
{
	test_fixture_start();

#ifndef _WIN32
	cfg.slow_fs_list = strdup("");
#endif

	run_test(test_unexistant_bookmark);
	run_test(test_all_valid_bookmarks_can_be_queried);
	run_test(test_regular_bmarks_are_global);
	run_test(test_sel_bmarks_are_local);

#ifndef _WIN32
	free(cfg.slow_fs_list);
	cfg.slow_fs_list = NULL;
#endif

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
