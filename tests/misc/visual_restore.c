#include <stic.h>

#include <string.h> /* strcpy() strdup() */

#include <test-utils.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/visual.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/sort.h"
#include "../../src/marks.h"

SETUP_ONCE()
{
	curr_view = &lwin;
	other_view = &rwin;
}

SETUP()
{
	modes_init();
	view_setup(&lwin);
	opt_handlers_setup();

	strcpy(lwin.curr_dir, TEST_DATA_PATH);
	lwin.list_rows = 3;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("file0");
	lwin.dir_entry[0].origin = lwin.curr_dir;
	lwin.dir_entry[0].selected = 0;
	lwin.dir_entry[1].name = strdup("file1");
	lwin.dir_entry[1].origin = lwin.curr_dir;
	lwin.dir_entry[1].selected = 0;
	lwin.dir_entry[2].name = strdup("file2");
	lwin.dir_entry[2].origin = lwin.curr_dir;
	lwin.dir_entry[2].selected = 0;
	lwin.selected_files = 0;

	view_set_sort(lwin.sort, SK_BY_INAME, SK_NONE);
	sort_view(&lwin);

	(void)vle_keys_exec_timed_out(WK_v);
	(void)vle_keys_exec_timed_out(WK_j);

	assert_int_equal(2, lwin.selected_files);
	assert_true(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);
	assert_false(lwin.dir_entry[2].selected);
	assert_int_equal(1, lwin.list_pos);

	(void)vle_keys_exec_timed_out(WK_ESC);

	assert_int_equal(0, lwin.selected_files);
	assert_false(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);
	assert_false(lwin.dir_entry[2].selected);
	assert_int_equal(1, lwin.list_pos);
}

TEARDOWN()
{
	modvis_leave(/*save_msg=*/0, /*goto_top=*/1, /*clear_selection=*/0);
	view_teardown(&lwin);
	opt_handlers_teardown();
	vle_keys_reset();
}

TEST(gv_works)
{
	modvis_enter(VS_RESTORE);

	assert_string_equal("VISUAL", modvis_describe());

	const mark_t *mark;
	mark = get_mark_by_name(&lwin, '<');
	assert_string_equal("file0", mark->file);
	mark = get_mark_by_name(&lwin, '>');
	assert_string_equal("file1", mark->file);

	assert_int_equal(0, marks_find_in_view(&lwin, '<'));
	assert_int_equal(1, marks_find_in_view(&lwin, '>'));

	assert_int_equal(2, lwin.selected_files);
	assert_true(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);
	assert_false(lwin.dir_entry[2].selected);
}

TEST(gv_works_after_inverting_the_order_of_files)
{
	const mark_t *mark;

	view_set_sort(lwin.sort, -SK_BY_INAME, SK_NONE);
	sort_view(&lwin);

	modvis_enter(VS_RESTORE);

	mark = get_mark_by_name(&lwin, '<');
	assert_string_equal("file1", mark->file);
	mark = get_mark_by_name(&lwin, '>');
	assert_string_equal("file0", mark->file);

	assert_int_equal(1, marks_find_in_view(&lwin, '<'));
	assert_int_equal(2, marks_find_in_view(&lwin, '>'));

	assert_int_equal(2, lwin.selected_files);
	assert_false(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);
	assert_true(lwin.dir_entry[2].selected);

	modvis_leave(/*save_msg=*/0, /*goto_top=*/1, /*clear_selection=*/0);

	view_set_sort(lwin.sort, SK_BY_INAME, SK_NONE);
	sort_view(&lwin);

	modvis_enter(VS_RESTORE);

	mark = get_mark_by_name(&lwin, '<');
	assert_string_equal("file0", mark->file);
	mark = get_mark_by_name(&lwin, '>');
	assert_string_equal("file1", mark->file);

	assert_int_equal(0, marks_find_in_view(&lwin, '<'));
	assert_int_equal(1, marks_find_in_view(&lwin, '>'));

	assert_int_equal(2, lwin.selected_files);
	assert_true(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);
	assert_false(lwin.dir_entry[2].selected);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
