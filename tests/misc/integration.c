#include <stic.h>

#include <sys/stat.h> /* chmod() */
#include <unistd.h> /* chdir() */

#include <stdio.h> /* fclose() fmemopen() remove() */
#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/engine/keys.h"
#include "../../src/int/term_title.h"
#include "../../src/int/vim.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/column_view.h"
#include "../../src/ui/fileview.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/macros.h"
#include "../../src/utils/str.h"
#include "../../src/event_loop.h"
#include "../../src/filelist.h"
#include "../../src/status.h"

SETUP()
{
	update_string(&cfg.shell, "");
	update_string(&cfg.slow_fs_list, "");
	assert_success(stats_init(&cfg));
}

TEARDOWN()
{
	update_string(&cfg.shell, NULL);
	update_string(&cfg.slow_fs_list, NULL);
}

/* Because of fmemopen(). */
#if !defined(_WIN32) && !defined(__APPLE__)

TEST(writing_directory_on_stdout_prints_newline)
{
	char out_buf[100] = { };
	char out_spec[] = "-";

	curr_stats.chosen_dir_out = out_spec;
	curr_stats.original_stdout = fmemopen(out_buf, sizeof(out_buf), "w");

	vim_write_dir("/some/path");

	fclose(curr_stats.original_stdout);
	curr_stats.original_stdout = NULL;
	curr_stats.chosen_dir_out = NULL;

	assert_string_equal("/some/path\n", out_buf);
}

TEST(writing_directory_to_a_file_prints_newline)
{
	char out_buf[100] = { };
	char out_spec[] = SANDBOX_PATH "/out";

	curr_stats.chosen_dir_out = out_spec;
	curr_stats.original_stdout = fmemopen(out_buf, sizeof(out_buf), "w");

	vim_write_dir("/some/path");

	fclose(curr_stats.original_stdout);
	curr_stats.original_stdout = NULL;
	curr_stats.chosen_dir_out = NULL;

	assert_int_equal(11, get_file_size(out_spec));
	assert_success(remove(out_spec));
}

TEST(paths_in_root_do_not_have_extra_slash_on_choosing)
{
	char file[] = "bin";
	char *files[] = { file };
	char out_buf[100] = { };
	char out_spec[] = "-";

	strcpy(lwin.curr_dir, "/");

	curr_stats.chosen_files_out = out_spec;
	curr_stats.original_stdout = fmemopen(out_buf, sizeof(out_buf), "w");

	flist_set_marking(&lwin, 1);
	vim_write_file_list(&lwin, 1, files);

	fclose(curr_stats.original_stdout);
	curr_stats.original_stdout = NULL;
	curr_stats.chosen_files_out = NULL;

	assert_string_equal("/bin\n", out_buf);
}

TEST(abs_paths_are_not_touched_on_choosing)
{
	char file[] = "/bin";
	char *files[] = { file };
	char out_buf[100] = { };
	char out_spec[] = "-";

	strcpy(lwin.curr_dir, "/etc");

	curr_stats.chosen_files_out = out_spec;
	curr_stats.original_stdout = fmemopen(out_buf, sizeof(out_buf), "w");

	flist_set_marking(&lwin, 1);
	vim_write_file_list(&lwin, 1, files);

	fclose(curr_stats.original_stdout);
	curr_stats.original_stdout = NULL;
	curr_stats.chosen_files_out = NULL;

	assert_string_equal("/bin\n", out_buf);
}

TEST(selection_can_be_chosen)
{
	char out_buf[100] = { };
	char out_spec[] = "-";

	char cwd[PATH_MAX + 1];
	char expected[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	view_setup(&lwin);

	strcpy(lwin.curr_dir, "/");
	strcpy(expected, "/bin\n");

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "bin");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);
	lwin.dir_entry[0].selected = 1;

	curr_stats.chosen_files_out = out_spec;
	curr_stats.original_stdout = fmemopen(out_buf, sizeof(out_buf), "w");

	flist_set_marking(&lwin, 1);
	vim_write_file_list(&lwin, 0, NULL);

	fclose(curr_stats.original_stdout);
	curr_stats.original_stdout = NULL;
	curr_stats.chosen_files_out = NULL;

	assert_string_equal(expected, out_buf);

	view_teardown(&lwin);
}

TEST(path_are_formed_right_on_choosing_in_cv)
{
	char file[] = "file";
	char *files[] = { file };
	char out_buf[100] = { };
	char out_spec[] = "-";

	char cwd[PATH_MAX + 1];
	char expected[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	view_setup(&lwin);

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "", cwd);
	make_abs_path(expected, sizeof(expected), SANDBOX_PATH, "file\n", cwd);

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, ".");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	curr_stats.chosen_files_out = out_spec;
	curr_stats.original_stdout = fmemopen(out_buf, sizeof(out_buf), "w");

	flist_set_marking(&lwin, 1);
	vim_write_file_list(&lwin, 1, files);

	fclose(curr_stats.original_stdout);
	curr_stats.original_stdout = NULL;
	curr_stats.chosen_files_out = NULL;

	assert_string_equal(expected, out_buf);

	view_teardown(&lwin);
}

#endif

TEST(writing_directory_does_not_happen_with_empty_or_null_spec)
{
	char out_spec[] = "";

	curr_stats.chosen_dir_out = out_spec;
	vim_write_dir("/some/path");

	curr_stats.chosen_dir_out = NULL;
	vim_write_dir("/some/path");
}

TEST(externally_edited_local_filter_is_applied, IF(not_windows))
{
	FILE *fp;
	char path[PATH_MAX + 1];

	char *const saved_cwd = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	curr_view = &lwin;
	other_view = &rwin;
	view_setup(&lwin);
	fview_setup();
	lwin.columns = columns_create();

	init_modes();
	opt_handlers_setup();

	update_string(&cfg.shell, "/bin/sh");
	stats_update_shell_type(cfg.shell);

	fp = fopen("./script", "w");
	fputs("#!/bin/sh\n", fp);
	fputs("echo read > $3\n", fp);
	fclose(fp);
	assert_success(chmod("script", 0777));

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, ".",
			saved_cwd);
	load_dir_list(&lwin, 0);

	curr_stats.exec_env_type = EET_EMULATOR;
	make_abs_path(path, sizeof(path), SANDBOX_PATH, "script", saved_cwd);
	update_string(&cfg.vi_command, path);

	curr_stats.load_stage = 2;
	(void)process_scheduled_updates_of_view(&lwin);
	curr_stats.load_stage = 0;
	assert_true(lwin.list_rows > 1);
	(void)vle_keys_exec_timed_out(L"q=");
	curr_stats.load_stage = 2;
	(void)process_scheduled_updates_of_view(&lwin);
	curr_stats.load_stage = 0;
	assert_int_equal(1, lwin.list_rows);

	update_string(&cfg.vi_command, NULL);
	update_string(&cfg.shell, NULL);
	assert_success(unlink(path));

	columns_free(lwin.columns);
	lwin.columns = NULL;
	view_teardown(&lwin);

	vle_keys_reset();
	opt_handlers_teardown();
	columns_teardown();

	restore_cwd(saved_cwd);
}

TEST(title_support_is_detected_correctly)
{
	static char *XTERM_LIKE[] = {
		"xterm", "xterm-256color", "xterm-termite", "xterm-anything",
		"rxvt", "rxvt-256color", "rxvt-unicode", "rxvt-anything",
		"aterm", "Eterm", "foot", "foot-direct"
	};

	static char *SCREEN_LIKE[] = {
		"screen", "screen-bce", "screen-256color", "screen-256color-bce",
		"screen-anything"
	};

#ifdef _WIN32
	unsigned int i;
	for(i = 0; i < ARRAY_LEN(XTERM_LIKE); ++i)
	{
		assert_int_equal(TK_REGULAR, title_kind_for_termenv(XTERM_LIKE[i]));
	}
	for(i = 0; i < ARRAY_LEN(SCREEN_LIKE); ++i)
	{
		assert_int_equal(TK_REGULAR, title_kind_for_termenv(SCREEN_LIKE[i]));
	}
	assert_int_equal(TK_REGULAR, title_kind_for_termenv("linux"));
	assert_int_equal(TK_REGULAR, title_kind_for_termenv("unknown"));
#else
	unsigned int i;
	for(i = 0; i < ARRAY_LEN(XTERM_LIKE); ++i)
	{
		assert_int_equal(TK_REGULAR, title_kind_for_termenv(XTERM_LIKE[i]));
	}
	for(i = 0; i < ARRAY_LEN(SCREEN_LIKE); ++i)
	{
		assert_int_equal(TK_SCREEN, title_kind_for_termenv(SCREEN_LIKE[i]));
	}
	assert_int_equal(TK_ABSENT, title_kind_for_termenv("linux"));
	assert_int_equal(TK_ABSENT, title_kind_for_termenv("unknown"));
#endif
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
