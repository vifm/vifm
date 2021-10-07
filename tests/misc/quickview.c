#include <stic.h>

#include <stdio.h> /* FILE fclose() fopen() */
#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/lua/vlua.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/quickview.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/file_streams.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/matchers.h"
#include "../../src/utils/str.h"
#include "../../src/utils/string_array.h"
#include "../../src/filelist.h"
#include "../../src/filetype.h"
#include "../../src/vcache.h"

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;

	opt_handlers_setup();
}

TEARDOWN()
{
	opt_handlers_teardown();
}

TEST(no_extra_line_with_extra_padding)
{
	cfg.extra_padding = 1;
	lwin.window_rows = 3;
	assert_int_equal(1, ui_qv_height(&lwin));
}

TEST(no_extra_line_without_extra_padding)
{
	cfg.extra_padding = 0;
	lwin.window_rows = 1;
	assert_int_equal(1, ui_qv_height(&lwin));
}

TEST(preview_can_match_agains_full_paths)
{
	char *error;
	matchers_t *ms;

	ft_init(NULL);

	assert_non_null(ms = matchers_alloc("{{*/*}}", 0, 1, "", &error));
	assert_null(error);

	ft_set_viewers(ms, "the-viewer");

	assert_string_equal("the-viewer",
			qv_get_viewer(TEST_DATA_PATH "/read/two-lines"));

	ft_reset(0);
}

TEST(preview_prg_overrules_fileviewer)
{
	char *error;
	matchers_t *ms;

	ft_init(NULL);

	assert_non_null(ms = matchers_alloc("file", 0, 1, "", &error));
	assert_null(error);

	ft_set_viewers(ms, "the-viewer");

	assert_string_equal("the-viewer", qv_get_viewer("file"));
	update_string(&curr_view->preview_prg, "override");
	assert_string_equal("override", qv_get_viewer("file"));

	ft_reset(0);
}

TEST(preview_enabled_if_possible)
{
	assert_success(qv_ensure_is_shown());
	assert_success(qv_ensure_is_shown());

	curr_stats.preview.on = 0;
	other_view->explore_mode = 1;
	assert_failure(qv_ensure_is_shown());
	other_view->explore_mode = 0;
}

TEST(preview_is_closed_on_request)
{
	assert_success(qv_ensure_is_shown());
	qv_hide();
	assert_false(curr_stats.preview.on);
}

TEST(macros_are_expanded_for_viewer)
{
	strcpy(curr_view->curr_dir, "echo");
	char *expanded = qv_expand_viewer(curr_view, "%d 1", /*macros=*/NULL);
	assert_string_equal("echo 1", expanded);
	free(expanded);
}

TEST(when_preview_can_be_shown)
{
	assert_true(qv_can_show());

	other_view->explore_mode = 1;
	assert_false(qv_can_show());
	other_view->explore_mode = 0;
	assert_true(qv_can_show());

	curr_stats.number_of_windows = 1;
	assert_false(qv_can_show());
	curr_stats.number_of_windows = 2;
	assert_true(qv_can_show());
}

TEST(quick_view_picks_entry)
{
	char origin[] = "/path";
	char name[] = "name";
	dir_entry_t entry = { .origin = origin, .name = name, .type = FT_REG };
	char path[PATH_MAX + 1];

	qv_get_path_to_explore(&entry, path, sizeof(path));
	assert_string_equal("/path/name", path);
}

TEST(quick_view_picks_current_directory)
{
	char origin[] = "/path";
	char name[] = "..";
	dir_entry_t entry = { .origin = origin, .name = name, .type = FT_DIR };
	char path[PATH_MAX + 1];

	qv_get_path_to_explore(&entry, path, sizeof(path));
	assert_string_equal("/path", path);
}

TEST(quick_view_picks_parent_directory_if_there_is_a_match)
{
	char origin[] = "/path";
	char name[] = "..";
	dir_entry_t entry = { .origin = origin, .name = name, .type = FT_DIR };
	char path[PATH_MAX + 1];

	char *error;
	matchers_t *ms = matchers_alloc("../", 0, 1, "", &error);
	assert_non_null(ms);
	ft_set_viewers(ms, "do something");

	qv_get_path_to_explore(&entry, path, sizeof(path));
	assert_string_equal("/path/..", path);

	ft_reset(0);
}

TEST(can_read_only_specified_number_of_lines)
{
	FILE *fp = fopen(TEST_DATA_PATH "/read/dos-line-endings", "r");

	char line[128];

	assert_non_null(get_line(fp, line, sizeof(line)));
	assert_string_equal("first line\n", line);

	int complete;
	strlist_t lines = read_lines(fp, 1, &complete);
	assert_int_equal(1, lines.nitems);
	assert_false(complete);
	assert_string_equal("second line", lines.items[0]);
	free_string_array(lines.items, lines.nitems);

	assert_non_null(get_line(fp, line, sizeof(line)));
	assert_string_equal("third line\n", line);

	fclose(fp);
}

TEST(no_switch_into_view_mode_of_hidden_pane)
{
	char cwd[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	curr_view = &lwin;
	other_view = &rwin;
	view_setup(&lwin);
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", cwd);
	populate_dir_list(&lwin, 0);
	view_setup(&rwin);
	make_abs_path(rwin.curr_dir, sizeof(rwin.curr_dir), TEST_DATA_PATH, "", cwd);
	populate_dir_list(&rwin, 1);
	init_modes();

	curr_stats.preview.on = 1;
	curr_stats.number_of_windows = 1;

	(void)vle_keys_exec_timed_out(WK_C_w WK_w);
	assert_true(vle_mode_is(NORMAL_MODE));
	(void)vle_keys_exec_timed_out(WK_q);
	assert_true(curr_view == &rwin);

	curr_stats.preview.on = 0;
	vle_keys_reset();
	view_teardown(&lwin);
	view_teardown(&rwin);
}

TEST(can_clean_via_plugin)
{
	curr_stats.vlua = vlua_init();
	view_setup(&lwin);
	init_view_list(&lwin);
	lwin.window_cols = 10;
	lwin.window_rows = 20;

	assert_success(vlua_run_string(curr_stats.vlua,
				"function clear(info) ginfo = info; return {} end"));
	assert_success(vlua_run_string(curr_stats.vlua,
				"vifm.addhandler{ name = 'clear', handler = clear }"));

	qv_cleanup(&lwin, "#vifmtest#clear %%");

	assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.command)"));
	assert_string_equal("#vifmtest#clear %", ui_sb_last());
	assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.x)"));
	assert_string_equal("-1", ui_sb_last());
	assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.y)"));
	assert_string_equal("-1", ui_sb_last());
	assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.width)"));
	assert_string_equal("10", ui_sb_last());
	assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.height)"));
	assert_string_equal("20", ui_sb_last());

	view_teardown(&lwin);
	vlua_finish(curr_stats.vlua);
	curr_stats.vlua = NULL;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
