#include <stic.h>

#include <stddef.h> /* NULL */
#include <string.h> /* strlen() */

#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/marks.h"

SETUP()
{
	cfg.slow_fs_list = strdup("");
	lwin.list_pos = 0;
	lwin.column_count = 1;
	rwin.list_pos = 0;
	rwin.column_count = 1;
}

TEARDOWN()
{
	free(cfg.slow_fs_list);
	cfg.slow_fs_list = NULL;
}

TEST(unexistant_bookmark)
{
	assert_int_equal(1, goto_bookmark(&lwin, 'b'));
}

TEST(all_valid_bookmarks_can_be_queried)
{
	const int bookmark_count = strlen(valid_bookmarks);
	int i;
	for(i = 0; i < bookmark_count; ++i)
	{
		assert_true(get_bookmark(i) != NULL);
	}
}

TEST(regular_bmarks_are_global)
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

TEST(sel_bmarks_are_local)
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
