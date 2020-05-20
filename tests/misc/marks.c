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
	assert_int_equal(1, marks_goto(&lwin, 'b'));
}

TEST(all_valid_marks_can_be_queried)
{
	const int bookmark_count = strlen(marks_all);
	int i;
	for(i = 0; i < bookmark_count; ++i)
	{
		assert_true(marks_by_index(&lwin, i) != NULL);
	}
}

TEST(regular_marks_are_global)
{
	char c;
	for(c = 'a'; c <= 'z'; ++c)
	{
		const mark_t *mark;

		marks_set_user(&lwin, c, "lpath", "lfile");
		marks_set_user(&rwin, c, "rpath", "rfile");

		mark = get_mark_by_name(&lwin, c);
		assert_string_equal("rpath", mark->directory);
		assert_string_equal("rfile", mark->file);

		mark = get_mark_by_name(&rwin, c);
		assert_string_equal("rpath", mark->directory);
		assert_string_equal("rfile", mark->file);
	}
}

TEST(sel_marks_are_local)
{
	const mark_t *mark;

	marks_set_special(&lwin, '<', "lpath", "lfile<");
	marks_set_special(&lwin, '>', "lpath", "lfile>");

	marks_set_special(&rwin, '<', "rpath", "rfile<");
	marks_set_special(&rwin, '>', "rpath", "rfile>");

	mark = get_mark_by_name(&lwin, '<');
	assert_string_equal("lpath", mark->directory);
	assert_string_equal("lfile<", mark->file);
	mark = get_mark_by_name(&lwin, '>');
	assert_string_equal("lpath", mark->directory);
	assert_string_equal("lfile>", mark->file);

	mark = get_mark_by_name(&rwin, '<');
	assert_string_equal("rpath", mark->directory);
	assert_string_equal("rfile<", mark->file);
	mark = get_mark_by_name(&rwin, '>');
	assert_string_equal("rpath", mark->directory);
	assert_string_equal("rfile>", mark->file);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
