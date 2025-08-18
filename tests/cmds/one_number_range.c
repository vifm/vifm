#include <stic.h>

#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/engine/cmds.h"
#include "../../src/ui/ui.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/flist_sel.h"

static int count_marked(view_t *view);

extern cmds_conf_t cmds_conf;

SETUP_ONCE()
{
	stub_colmgr();
}

SETUP()
{
	view_setup(&lwin);
	view_setup(&rwin);

	strcpy(lwin.curr_dir, "/lwin");
	append_view_entry(&lwin, "..");
	append_view_entry(&lwin, "lfile0")->selected = 1;
	append_view_entry(&lwin, "lfile1");
	append_view_entry(&lwin, "lfile2")->selected = 1;
	append_view_entry(&lwin, "lfile3");
	lwin.list_pos = 2;
	lwin.selected_files = 2;

	strcpy(rwin.curr_dir, "/rwin");
	append_view_entry(&rwin, "..");
	append_view_entry(&rwin, "rfile0");
	append_view_entry(&rwin, "rfile1")->selected = 1;
	append_view_entry(&rwin, "rfile2");
	append_view_entry(&rwin, "rfile3")->selected = 1;
	append_view_entry(&rwin, "rfile4");
	append_view_entry(&rwin, "rfile5")->selected = 1;
	rwin.list_pos = 5;
	rwin.selected_files = 3;

	lwin.pending_marking = 0;
	rwin.pending_marking = 0;
}

TEARDOWN()
{
	view_teardown(&lwin);
	view_teardown(&rwin);
}

TEST(one_number_range)
{
	cmd_info_t cmd_info = {
		.begin = -1, .end = -1
	};

	curr_view = &lwin;
	cmds_conf.begin = 0;
	cmds_conf.current = curr_view->list_pos;
	cmds_conf.end = curr_view->list_rows - 1;
	cmds_select_range(-1, &cmd_info);
	assert_int_equal(2, count_marked(&lwin));

	curr_view = &rwin;
	cmds_conf.begin = 0;
	cmds_conf.current = curr_view->list_pos;
	cmds_conf.end = curr_view->list_rows - 1;
	cmds_select_range(-1, &cmd_info);
	assert_int_equal(3, count_marked(&rwin));
}

TEST(one_in_the_range)
{
	cmd_info_t cmd_info = {
		.begin = 1, .end = 1
	};

	curr_view = &lwin;
	cmds_conf.begin = 0;
	cmds_conf.current = curr_view->list_pos;
	cmds_conf.end = curr_view->list_rows - 1;
	cmds_select_range(-1, &cmd_info);
	assert_int_equal(1, count_marked(&lwin));
	assert_true(lwin.dir_entry[1].selected);

	curr_view = &rwin;
	cmds_conf.begin = 0;
	cmds_conf.current = curr_view->list_pos;
	cmds_conf.end = curr_view->list_rows - 1;
	cmds_select_range(-1, &cmd_info);
	assert_int_equal(1, count_marked(&rwin));
	assert_true(rwin.dir_entry[1].marked);
}

TEST(parent_directory_is_not_selected)
{
	cmd_info_t cmd_info = {
		.begin = 0, .end = 0
	};

	curr_view = &lwin;

	cmds_conf.begin = 0;
	cmds_conf.current = curr_view->list_pos;
	cmds_conf.end = curr_view->list_rows - 1;

	cmds_select_range(-1, &cmd_info);
	assert_int_equal(0, count_marked(&lwin));
	assert_false(lwin.dir_entry[0].selected);

	flist_sel_drop(&lwin);

	cmd_info.begin = NOT_DEF;
	cmds_select_range(-1, &cmd_info);
	assert_int_equal(0, count_marked(&lwin));
	assert_false(lwin.dir_entry[0].selected);

	cmd_info.end = NOT_DEF;
	cmds_conf.current = 0;
	curr_view->list_pos = 0;
	cmds_select_range(-1, &cmd_info);
	assert_int_equal(0, count_marked(&lwin));
	assert_false(lwin.dir_entry[0].selected);
}

TEST(range_of_previous_command_is_reset)
{
	curr_view = &lwin;
	cmds_conf.begin = 0;
	cmds_conf.current = curr_view->list_pos;
	cmds_conf.end = curr_view->list_rows - 1;

	cmd_info_t cmd_info1 = { .begin = 1, .end = 2 };
	cmds_select_range(-1, &cmd_info1);
	assert_int_equal(2, count_marked(&lwin));

	cmd_info_t cmd_info2 = { .begin = 3, .end = 4 };
	cmds_select_range(-1, &cmd_info2);
	assert_int_equal(2, count_marked(&lwin));
}

static int
count_marked(view_t *view)
{
	check_marking(view, 0, NULL);
	return flist_count_marked(view);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
