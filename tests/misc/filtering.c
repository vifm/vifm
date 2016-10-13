#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../../src/cfg/config.h"
#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/filtering.h"
#include "../../src/opt_handlers.h"
#include "../../src/status.h"

#include "utils.h"

#define assert_hidden(view, name, dir) \
	assert_false(filters_file_is_visible(&view, name, dir))

#define assert_visible(view, name, dir) \
	assert_true(filters_file_is_visible(&view, name, dir))

SETUP()
{
	cfg.slow_fs_list = strdup("");

	cfg.filter_inverted_by_default = 1;

	view_setup(&lwin);

	lwin.list_rows = 7;
	lwin.list_pos = 2;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("with(round)");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].name = strdup("with[square]");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];
	lwin.dir_entry[2].name = strdup("with{curly}");
	lwin.dir_entry[2].origin = &lwin.curr_dir[0];
	lwin.dir_entry[3].name = strdup("with<angle>");
	lwin.dir_entry[3].origin = &lwin.curr_dir[0];
	lwin.dir_entry[4].name = strdup("withSPECS+*^$?|\\");
	lwin.dir_entry[4].origin = &lwin.curr_dir[0];
	lwin.dir_entry[5].name = strdup("with....dots");
	lwin.dir_entry[5].origin = &lwin.curr_dir[0];
	lwin.dir_entry[6].name = strdup("withnonodots");
	lwin.dir_entry[6].origin = &lwin.curr_dir[0];

	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].selected = 1;
	lwin.dir_entry[2].selected = 1;
	lwin.dir_entry[3].selected = 1;
	lwin.dir_entry[4].selected = 1;
	lwin.dir_entry[5].selected = 1;
	lwin.dir_entry[6].selected = 0;
	lwin.selected_files = 6;

	lwin.invert = cfg.filter_inverted_by_default;

	lwin.column_count = 1;

	view_setup(&rwin);

	rwin.list_rows = 8;
	rwin.list_pos = 2;
	rwin.dir_entry = dynarray_cextend(NULL,
			rwin.list_rows*sizeof(*rwin.dir_entry));
	rwin.dir_entry[0].name = strdup("dir1.d");
	rwin.dir_entry[0].origin = &rwin.curr_dir[0];
	rwin.dir_entry[1].name = strdup("dir2.d");
	rwin.dir_entry[1].origin = &rwin.curr_dir[0];
	rwin.dir_entry[2].name = strdup("dir3.d");
	rwin.dir_entry[2].origin = &rwin.curr_dir[0];
	rwin.dir_entry[3].name = strdup("file1.d");
	rwin.dir_entry[3].origin = &rwin.curr_dir[0];
	rwin.dir_entry[4].name = strdup("file2.d");
	rwin.dir_entry[4].origin = &rwin.curr_dir[0];
	rwin.dir_entry[5].name = strdup("file3.d");
	rwin.dir_entry[5].origin = &rwin.curr_dir[0];
	rwin.dir_entry[6].name = strdup("withnonodots");
	rwin.dir_entry[6].origin = &rwin.curr_dir[0];
	rwin.dir_entry[7].name = strdup("somedir");
	rwin.dir_entry[7].origin = &rwin.curr_dir[0];

	rwin.dir_entry[0].selected = 0;
	rwin.dir_entry[1].selected = 0;
	rwin.dir_entry[2].selected = 0;
	rwin.dir_entry[3].selected = 0;
	rwin.dir_entry[4].selected = 0;
	rwin.dir_entry[5].selected = 0;
	rwin.dir_entry[6].selected = 0;
	rwin.dir_entry[7].selected = 0;
	rwin.selected_files = 0;

	rwin.invert = cfg.filter_inverted_by_default;

	rwin.column_count = 1;
}

TEARDOWN()
{
	update_string(&cfg.slow_fs_list, NULL);

	view_teardown(&lwin);
	view_teardown(&rwin);
}

TEST(filtering)
{
	assert_int_equal(7, lwin.list_rows);
	filter_selected_files(&lwin);
	assert_int_equal(1, lwin.list_rows);

	assert_string_equal("withnonodots", lwin.dir_entry[0].name);
	assert_visible(lwin, lwin.dir_entry[0].name, 0);
}

TEST(filtering_file_does_not_filter_dir)
{
	char *const name = strdup(rwin.dir_entry[6].name);

	rwin.dir_entry[6].selected = 1;
	rwin.selected_files = 1;

	assert_int_equal(8, rwin.list_rows);
	filter_selected_files(&rwin);
	assert_int_equal(7, rwin.list_rows);

	assert_hidden(rwin, name, 0);
	assert_visible(rwin, name, 1);

	free(name);
}

TEST(filtering_dir_does_not_filter_file)
{
	char *const name = strdup(rwin.dir_entry[6].name);

	rwin.dir_entry[6].selected = 1;
	rwin.dir_entry[6].type = FT_DIR;
	rwin.selected_files = 1;

	assert_int_equal(8, rwin.list_rows);
	filter_selected_files(&rwin);
	assert_int_equal(7, rwin.list_rows);

	assert_hidden(rwin, name, 1);
	assert_visible(rwin, name, 0);

	free(name);
}

TEST(filtering_files_does_not_filter_dirs)
{
	(void)filter_set(&rwin.manual_filter, "^.*\\.d$");

	assert_visible(rwin, rwin.dir_entry[0].name, 1);
	assert_visible(rwin, rwin.dir_entry[1].name, 1);
	assert_visible(rwin, rwin.dir_entry[2].name, 1);
	assert_hidden(rwin, rwin.dir_entry[3].name, 0);
	assert_hidden(rwin, rwin.dir_entry[4].name, 0);
	assert_hidden(rwin, rwin.dir_entry[5].name, 0);
	assert_visible(rwin, rwin.dir_entry[6].name, 0);
	assert_visible(rwin, rwin.dir_entry[7].name, 1);
	assert_int_equal(8, rwin.list_rows);
}

TEST(filtering_dirs_does_not_filter_files)
{
	(void)filter_set(&rwin.manual_filter, "^.*\\.d/$");

	assert_hidden(rwin, rwin.dir_entry[0].name, 1);
	assert_hidden(rwin, rwin.dir_entry[1].name, 1);
	assert_hidden(rwin, rwin.dir_entry[2].name, 1);
	assert_visible(rwin, rwin.dir_entry[3].name, 0);
	assert_visible(rwin, rwin.dir_entry[4].name, 0);
	assert_visible(rwin, rwin.dir_entry[5].name, 0);
	assert_visible(rwin, rwin.dir_entry[6].name, 0);
	assert_visible(rwin, rwin.dir_entry[7].name, 1);
	assert_int_equal(8, rwin.list_rows);
}

TEST(filtering_files_and_dirs)
{
	(void)filter_set(&rwin.manual_filter, "^.*\\.d/?$");

	assert_hidden(rwin, rwin.dir_entry[0].name, 1);
	assert_hidden(rwin, rwin.dir_entry[1].name, 1);
	assert_hidden(rwin, rwin.dir_entry[2].name, 1);
	assert_hidden(rwin, rwin.dir_entry[3].name, 0);
	assert_hidden(rwin, rwin.dir_entry[4].name, 0);
	assert_hidden(rwin, rwin.dir_entry[5].name, 0);
	assert_visible(rwin, rwin.dir_entry[6].name, 0);
	assert_visible(rwin, rwin.dir_entry[7].name, 1);
	assert_int_equal(8, rwin.list_rows);
}

TEST(file_after_directory_is_hidden)
{
	view_teardown(&lwin);
	view_setup(&lwin);

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/read");
	flist_custom_add(&lwin, TEST_DATA_PATH "/read/very-long-line");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	lwin.dir_entry[1].selected = 1;
	lwin.selected_files = 1;
	filter_selected_files(&lwin);

	assert_int_equal(1, lwin.list_rows);
}

TEST(global_local_nature_of_normal_zo)
{
	view_teardown(&lwin);
	view_setup(&lwin);

	view_teardown(&rwin);
	view_setup(&rwin);

	curr_view = &lwin;
	other_view = &rwin;

	lwin.hide_dot_g = lwin.hide_dot = 0;
	rwin.hide_dot_g = rwin.hide_dot = 1;

	opt_handlers_setup();
	load_view_options(curr_view);

	init_modes();

	curr_stats.global_local_settings = 1;

	assert_success(exec_commands("normal zo", &lwin, CIT_COMMAND));
	assert_false(lwin.hide_dot_g);
	assert_false(lwin.hide_dot);
	assert_false(rwin.hide_dot_g);
	assert_false(rwin.hide_dot);

	curr_stats.global_local_settings = 0;

	vle_keys_reset();
	reset_cmds();
	opt_handlers_teardown();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
