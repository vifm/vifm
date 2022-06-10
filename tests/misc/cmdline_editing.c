#include <stic.h>

#include <ctype.h> /* isspace() */
#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() */
#include <string.h> /* memset() strcpy() strdup() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/curses.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/engine/abbrevs.h"
#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/modes/cmdline.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/matcher.h"
#include "../../src/utils/str.h"
#include "../../src/builtin_functions.h"
#include "../../src/filelist.h"
#include "../../src/status.h"

static int have_ext_keys(void);

static line_stats_t *stats;

SETUP_ONCE()
{
	stats = get_line_stats();
	try_enable_utf8_locale();
	init_builtin_functions();
}

SETUP()
{
	init_modes();
	modcline_enter(CLS_COMMAND, "", NULL);

	curr_view = &lwin;
	view_setup(&lwin);

	char *error;
	matcher_free(curr_view->manual_filter);
	assert_non_null(curr_view->manual_filter =
			matcher_alloc("{filt}", 0, 0, "", &error));
	assert_success(filter_set(&curr_view->auto_filter, "auto-filter"));
	assert_success(filter_set(&curr_view->local_filter.filter, "local-filter"));

	lwin.list_pos = 0;
	lwin.list_rows = 2;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("\265");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].name = strdup("root.ext");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];
	strcpy(lwin.curr_dir, "tests/fake/tail");

	other_view = &rwin;
	view_setup(&rwin);

	rwin.list_pos = 0;
	rwin.list_rows = 1;
	rwin.dir_entry = dynarray_cextend(NULL,
			rwin.list_rows*sizeof(*rwin.dir_entry));
	rwin.dir_entry[0].name = strdup("otherroot.otherext");
	rwin.dir_entry[0].origin = &rwin.curr_dir[0];
	strcpy(rwin.curr_dir, "other/dir/othertail");

	opt_handlers_setup();
}

TEARDOWN()
{
	view_teardown(&lwin);
	view_teardown(&rwin);

	(void)vle_keys_exec_timed_out(WK_C_c);

	vle_keys_reset();

	opt_handlers_teardown();
}

TEST(backspace_exit)
{
	assert_true(vle_mode_is(CMDLINE_MODE));
	assert_wstring_equal(L"", stats->line);

	(void)vle_keys_exec_timed_out(WK_C_h);
	assert_true(vle_mode_is(NORMAL_MODE));
	assert_wstring_equal(NULL, stats->line);
}

TEST(backspace_eol)
{
	(void)vle_keys_exec_timed_out(L"abc");
	assert_wstring_equal(L"abc", stats->line);

	(void)vle_keys_exec_timed_out(WK_C_h);
	assert_wstring_equal(L"ab", stats->line);

	(void)vle_keys_exec_timed_out(WK_C_h);
	assert_wstring_equal(L"a", stats->line);

	(void)vle_keys_exec_timed_out(WK_C_h);
	assert_wstring_equal(L"", stats->line);
}

TEST(backspace_not_eol)
{
	(void)vle_keys_exec_timed_out(L"abc");
	assert_wstring_equal(L"abc", stats->line);

	(void)vle_keys_exec_timed_out(WK_C_b);
	assert_wstring_equal(L"abc", stats->line);

	(void)vle_keys_exec_timed_out(WK_C_h);
	assert_wstring_equal(L"ac", stats->line);

	(void)vle_keys_exec_timed_out(WK_C_h);
	assert_wstring_equal(L"c", stats->line);

	(void)vle_keys_exec_timed_out(WK_C_h);
	assert_wstring_equal(L"c", stats->line);
}

TEST(right_kill)
{
	(void)vle_keys_exec_timed_out(L"abc");
	assert_wstring_equal(L"abc", stats->line);

	(void)vle_keys_exec_timed_out(WK_C_k);
	assert_wstring_equal(L"abc", stats->line);

	(void)vle_keys_exec_timed_out(WK_C_h);
	(void)vle_keys_exec_timed_out(WK_C_k);
	assert_wstring_equal(L"ab", stats->line);

	(void)vle_keys_exec_timed_out(WK_C_a);
	(void)vle_keys_exec_timed_out(WK_C_k);
	assert_wstring_equal(L"", stats->line);

	(void)vle_keys_exec_timed_out(WK_C_k);
	assert_wstring_equal(L"", stats->line);
}

TEST(left_kill)
{
	(void)vle_keys_exec_timed_out(L"abc");
	assert_wstring_equal(L"abc", stats->line);

	(void)vle_keys_exec_timed_out(WK_C_b);
	(void)vle_keys_exec_timed_out(WK_C_b);
	assert_wstring_equal(L"abc", stats->line);

	(void)vle_keys_exec_timed_out(WK_C_u);
	assert_wstring_equal(L"bc", stats->line);

	(void)vle_keys_exec_timed_out(WK_C_f);
	(void)vle_keys_exec_timed_out(WK_C_u);
	assert_wstring_equal(L"c", stats->line);

	(void)vle_keys_exec_timed_out(WK_C_f);
	(void)vle_keys_exec_timed_out(WK_C_u);
	assert_wstring_equal(L"", stats->line);

	(void)vle_keys_exec_timed_out(WK_C_u);
	assert_wstring_equal(L"", stats->line);
}

TEST(history)
{
	cfg.history_len = 4;
	assert_success(stats_init(&cfg));

	(void)vle_keys_exec_timed_out(L"first");
	assert_wstring_equal(L"first", stats->line);
	(void)vle_keys_exec_timed_out(WK_ESC);

	(void)vle_keys_exec_timed_out(L":second");
	assert_wstring_equal(L"second", stats->line);
	(void)vle_keys_exec_timed_out(WK_ESC);

	(void)vle_keys_exec_timed_out(L":third" WK_C_p);
	assert_wstring_equal(L"second", stats->line);
	(void)vle_keys_exec_timed_out(WK_C_p);
	assert_wstring_equal(L"first", stats->line);
	(void)vle_keys_exec_timed_out(WK_C_p);
	assert_wstring_equal(L"first", stats->line);

	(void)vle_keys_exec_timed_out(WK_C_n);
	assert_wstring_equal(L"second", stats->line);
	(void)vle_keys_exec_timed_out(WK_C_n);
	assert_wstring_equal(L"third", stats->line);
	(void)vle_keys_exec_timed_out(WK_C_n);
	assert_wstring_equal(L"third", stats->line);

	hists_resize(0);
	cfg.history_len = 0;
	assert_success(stats_reset(&cfg));
}

TEST(prefix_history, IF(have_ext_keys))
{
	const wchar_t up[] = {K(KEY_UP), 0};
	const wchar_t down[] = {K(KEY_DOWN), 0};

	cfg.history_len = 4;
	assert_success(stats_init(&cfg));

	(void)vle_keys_exec_timed_out(L"first");
	assert_wstring_equal(L"first", stats->line);
	(void)vle_keys_exec_timed_out(WK_ESC);

	(void)vle_keys_exec_timed_out(L":second");
	assert_wstring_equal(L"second", stats->line);
	(void)vle_keys_exec_timed_out(WK_ESC);

	(void)vle_keys_exec_timed_out(L":finish");
	assert_wstring_equal(L"finish", stats->line);
	(void)vle_keys_exec_timed_out(WK_ESC);

	(void)vle_keys_exec_timed_out(L":fi");
	assert_wstring_equal(L"fi", stats->line);
	(void)vle_keys_exec_timed_out(up);
	assert_wstring_equal(L"finish", stats->line);
	(void)vle_keys_exec_timed_out(up);
	assert_wstring_equal(L"first", stats->line);

	(void)vle_keys_exec_timed_out(down);
	assert_wstring_equal(L"finish", stats->line);
	(void)vle_keys_exec_timed_out(down);
	assert_wstring_equal(L"fi", stats->line);
	(void)vle_keys_exec_timed_out(down);
	assert_wstring_equal(L"fi", stats->line);

	hists_resize(0);
	cfg.history_len = 0;
	assert_success(stats_reset(&cfg));
}

TEST(value_of_manual_filter_is_pasted)
{
	(void)vle_keys_exec_timed_out(WK_C_x WK_m);
	assert_wstring_equal(L"filt", stats->line);
}

TEST(value_of_automatic_filter_is_pasted)
{
	(void)vle_keys_exec_timed_out(WK_C_x WK_a);
	assert_wstring_equal(L"auto-filter", stats->line);
}

TEST(value_of_local_filter_is_pasted)
{
	(void)vle_keys_exec_timed_out(WK_C_x WK_EQUALS);
	assert_wstring_equal(L"local-filter", stats->line);
}

TEST(last_search_pattern_is_pasted)
{
	/* Emulate proper history initialization (must happen after view
	 * initialization). */
	cfg_resize_histories(5);
	cfg_resize_histories(0);

	cfg_resize_histories(5);
	hists_search_save("search-pattern");

	(void)vle_keys_exec_timed_out(WK_C_x WK_SLASH);
	assert_wstring_equal(L"search-pattern", stats->line);

	cfg_resize_histories(0);
}

TEST(name_of_current_file_is_pasted)
{
	lwin.list_pos = 1;

	(void)vle_keys_exec_timed_out(WK_C_x WK_c);
	assert_wstring_equal(L"root.ext", stats->line);

	(void)vle_keys_exec_timed_out(WK_SPACE);

	(void)vle_keys_exec_timed_out(WK_C_x WK_C_x WK_c);
	assert_wstring_equal(L"root.ext otherroot.otherext", stats->line);
}

TEST(short_path_of_current_file_is_pasted_in_cv)
{
	char cwd[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", cwd);

	char path[PATH_MAX + 1];
	flist_custom_start(&lwin, "test");
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "existing-files/a", cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "existing-files/b", cwd);
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	(void)vle_keys_exec_timed_out(WK_C_x WK_c);

	assert_wstring_equal(L"existing-files/a", stats->line);
}

TEST(root_is_pasted)
{
	lwin.list_pos = 1;

	(void)vle_keys_exec_timed_out(WK_C_x WK_r);
	assert_wstring_equal(L"root", stats->line);

	(void)vle_keys_exec_timed_out(WK_SPACE);

	(void)vle_keys_exec_timed_out(WK_C_x WK_C_x WK_r);
	assert_wstring_equal(L"root otherroot", stats->line);
}

TEST(extension_is_pasted)
{
	lwin.list_pos = 1;

	(void)vle_keys_exec_timed_out(WK_C_x WK_e);
	assert_wstring_equal(L"ext", stats->line);

	(void)vle_keys_exec_timed_out(WK_SPACE);

	(void)vle_keys_exec_timed_out(WK_C_x WK_C_x WK_e);
	assert_wstring_equal(L"ext otherext", stats->line);
}

TEST(directory_is_pasted)
{
	(void)vle_keys_exec_timed_out(WK_C_x WK_d);
	assert_wstring_equal(L"tests/fake/tail", stats->line);

	(void)vle_keys_exec_timed_out(WK_SPACE);

	(void)vle_keys_exec_timed_out(WK_C_x WK_C_x WK_d);
	assert_wstring_equal(L"tests/fake/tail other/dir/othertail", stats->line);
}

TEST(tail_of_current_directory_is_pasted)
{
	(void)vle_keys_exec_timed_out(WK_C_x WK_t);
	assert_wstring_equal(L"tail", stats->line);

	(void)vle_keys_exec_timed_out(WK_SPACE);

	(void)vle_keys_exec_timed_out(WK_C_x WK_C_x WK_t);
	assert_wstring_equal(L"tail othertail", stats->line);
}

TEST(broken_utf8_name, IF(utf8_locale))
{
	(void)vle_keys_exec_timed_out(WK_C_x WK_c);

	assert_true(stats->line != NULL && stats->line[0] != L'\0');
}

TEST(last_argument_is_inserted)
{
#ifndef __PDCURSES__
	const wchar_t meta_dot[] = WK_ESC WK_DOT;
#else
	const wchar_t meta_dot[] = { K(ALT_PERIOD), L'\0' };
#endif

	/* Emulate proper history initialization (must happen after view
	 * initialization). */
	cfg_resize_histories(5);
	cfg_resize_histories(0);

	cfg_resize_histories(5);
	hists_commands_save("a1");
	hists_commands_save("b1 b2");
	hists_commands_save("c1 c2 c3");
	hists_commands_save("d1 d2 d3 d4");

	(void)vle_keys_exec_timed_out(meta_dot);
	assert_wstring_equal(L"d4", stats->line);
	(void)vle_keys_exec_timed_out(meta_dot);
	assert_wstring_equal(L"c3", stats->line);
	(void)vle_keys_exec_timed_out(meta_dot);
	assert_wstring_equal(L"b2", stats->line);
	(void)vle_keys_exec_timed_out(meta_dot);
	assert_wstring_equal(L"a1", stats->line);
	(void)vle_keys_exec_timed_out(meta_dot);
	assert_wstring_equal(L"", stats->line);
	(void)vle_keys_exec_timed_out(meta_dot);
	assert_wstring_equal(L"", stats->line);

	(void)vle_keys_exec_timed_out(WK_SPACE);
	assert_wstring_equal(L" ", stats->line);
	(void)vle_keys_exec_timed_out(meta_dot);
	assert_wstring_equal(L" d4", stats->line);

	cfg_resize_histories(0);
}

TEST(abbrevs_are_expanded)
{
	int i;
	for(i = 0; i < 255; ++i)
	{
		cfg.word_chars[i] = !isspace(i);
	}

	assert_success(vle_abbr_add(L"lhs", L"rhs"));

	(void)vle_keys_exec_timed_out(L"lhs");
	assert_wstring_equal(L"lhs", stats->line);

	(void)vle_keys_exec_timed_out(L" ");
	assert_wstring_equal(L"rhs ", stats->line);

	vle_abbr_reset();

	memset(cfg.word_chars, 0, sizeof(cfg.word_chars));
}

TEST(expr_reg_good_expr)
{
	(void)vle_keys_exec_timed_out(L"ad" WK_C_b);
	(void)vle_keys_exec_timed_out(WK_C_r WK_EQUALS);
	(void)vle_keys_exec_timed_out(L"'bc'" WK_CR);
	assert_wstring_equal(L"abcd", stats->line);
}

TEST(expr_reg_bad_expr)
{
	(void)vle_keys_exec_timed_out(L"ad" WK_C_b);
	(void)vle_keys_exec_timed_out(WK_C_r WK_EQUALS);
	(void)vle_keys_exec_timed_out(L"bc" WK_CR);
	assert_wstring_equal(L"ad", stats->line);
}

/* This tests requires some interactivity and full command-line mode similar to
 * editing, hence it's here. */
TEST(expr_reg_completion)
{
	(void)vle_keys_exec_timed_out(L":" WK_C_r WK_EQUALS);
	(void)vle_keys_exec_timed_out(L"ex" WK_C_i);
	assert_wstring_equal(L"executable(", stats->line);
	(void)vle_keys_exec_timed_out(WK_C_c);
}

/* This tests requires some interactivity and full command-line mode similar to
 * editing, hence it's here. */
TEST(expr_reg_completion_ignores_pipe)
{
	(void)vle_keys_exec_timed_out(L":" WK_C_r WK_EQUALS);
	(void)vle_keys_exec_timed_out(L"ab|ex" WK_C_i);
	assert_wstring_equal(L"ab|ex", stats->line);
	(void)vle_keys_exec_timed_out(WK_C_c);
}

TEST(ext_edited_prompt_is_saved_to_history, IF(not_windows))
{
	FILE *fp;
	char path[PATH_MAX + 1];

	char *const saved_cwd = save_cwd();
	assert_success(os_chdir(SANDBOX_PATH));

	/* Emulate proper history initialization (must happen after view
	 * initialization). */
	cfg_resize_histories(10);
	cfg_resize_histories(0);
	cfg_resize_histories(10);

	update_string(&cfg.shell, "/bin/sh");
	stats_update_shell_type(cfg.shell);

	fp = fopen("./script", "w");
	fputs("#!/bin/sh\n", fp);
	fputs("echo \\'ext-edit\\' > $3\n", fp);
	fclose(fp);
	assert_success(os_chmod("script", 0777));

	curr_stats.exec_env_type = EET_EMULATOR;
	make_abs_path(path, sizeof(path), SANDBOX_PATH, "script", saved_cwd);
	update_string(&cfg.vi_command, path);

	(void)vle_keys_exec_timed_out(WK_C_r WK_EQUALS);
	(void)vle_keys_exec_timed_out(WK_C_g);

	assert_wstring_equal(L"ext-edit", stats->line);

	update_string(&cfg.vi_command, NULL);
	update_string(&cfg.shell, NULL);
	assert_success(unlink(path));

	restore_cwd(saved_cwd);

	assert_int_equal(1, curr_stats.exprreg_hist.size);
	assert_string_equal("'ext-edit'", curr_stats.exprreg_hist.items[0].text);

	cfg_resize_histories(0);
}

static int
have_ext_keys(void)
{
#ifdef ENABLE_EXTENDED_KEYS
	return 1;
#else
	return 0;
#endif
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
