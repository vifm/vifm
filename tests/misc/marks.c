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

TEST(unexistent_mark)
{
	assert_int_equal(1, goto_mark(&lwin, 'b'));
}

TEST(all_valid_marks_can_be_queried)
{
	const int bookmark_count = strlen(valid_marks);
	int i;
	for(i = 0; i < bookmark_count; ++i)
	{
		assert_true(get_mark(i) != NULL);
	}
}

TEST(regular_marks_are_global)
{
	const mark_t *mark;

	curr_view = &lwin;
	set_user_mark('a', "lpath", "lfile");

	curr_view = &rwin;
	set_user_mark('a', "rpath", "rfile");

	curr_view = &lwin;
	mark = get_mark_by_name('a');
	assert_string_equal("rpath", mark->directory);
	assert_string_equal("rfile", mark->file);

	curr_view = &rwin;
	mark = get_mark_by_name('a');
	assert_string_equal("rpath", mark->directory);
	assert_string_equal("rfile", mark->file);
}

TEST(sel_marks_are_local)
{
	const mark_t *mark;

	curr_view = &lwin;
	set_spec_mark('<', "lpath", "lfile<");
	set_spec_mark('>', "lpath", "lfile>");

	curr_view = &rwin;
	set_spec_mark('<', "rpath", "rfile<");
	set_spec_mark('>', "rpath", "rfile>");

	curr_view = &lwin;
	mark = get_mark_by_name('<');
	assert_string_equal("lpath", mark->directory);
	assert_string_equal("lfile<", mark->file);
	mark = get_mark_by_name('>');
	assert_string_equal("lpath", mark->directory);
	assert_string_equal("lfile>", mark->file);

	curr_view = &rwin;
	mark = get_mark_by_name('<');
	assert_string_equal("rpath", mark->directory);
	assert_string_equal("rfile<", mark->file);
	mark = get_mark_by_name('>');
	assert_string_equal("rpath", mark->directory);
	assert_string_equal("rfile>", mark->file);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
