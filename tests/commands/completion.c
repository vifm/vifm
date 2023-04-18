#include <stic.h>

#include <unistd.h> /* chdir() rmdir() */

#include <stddef.h> /* NULL */
#include <stdlib.h> /* fclose() fopen() free() */
#include <string.h> /* strdup() strstr() */
#include <wchar.h> /* wcsdup() wcslen() */

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/cfg/config.h"
#include "../../src/engine/abbrevs.h"
#include "../../src/engine/cmds.h"
#include "../../src/engine/completion.h"
#include "../../src/engine/functions.h"
#include "../../src/engine/options.h"
#include "../../src/engine/variables.h"
#include "../../src/int/path_env.h"
#include "../../src/lua/vlua.h"
#include "../../src/modes/cmdline.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/env.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/bmarks.h"
#include "../../src/builtin_functions.h"
#include "../../src/cmd_core.h"
#include "../../src/plugins.h"

#define ASSERT_COMPLETION(initial, expected) \
	do \
	{ \
		prepare_for_line_completion(initial); \
		assert_success(line_completion(&stats)); \
		assert_wstring_equal(expected, stats.line); \
	} \
	while (0)

#define ASSERT_NO_COMPLETION(initial) ASSERT_COMPLETION((initial), (initial))

#define ASSERT_NEXT_MATCH(str) \
	do \
	{ \
		char *const buf = vle_compl_next(); \
		assert_string_equal((str), buf); \
		free(buf); \
	} \
	while (0)

static void dummy_handler(OPT_OP op, optval_t val);
static void prepare_for_line_completion(const wchar_t str[]);

static line_stats_t stats;
static char *saved_cwd;

SETUP()
{
	static int option_changed;
	optval_t def = { .str_val = "/tmp" };

	cfg.slow_fs_list = strdup("");

	init_builtin_functions();

	stats.line = wcsdup(L"set ");
	stats.index = wcslen(stats.line);
	stats.curs_pos = 0;
	stats.len = stats.index;
	stats.cmd_pos = -1;
	stats.complete_continue = 0;
	stats.history_search = 0;
	stats.line_buf = NULL;
	stats.complete = &vle_cmds_complete;

	curr_view = &lwin;

	cmds_init();

	vle_opts_init(&option_changed, NULL);
	vle_opts_add("fusehome", "fh", "descr", OPT_STR, OPT_GLOBAL, 0, NULL,
			&dummy_handler, def);
	vle_opts_add("path", "pt", "descr", OPT_STR, OPT_GLOBAL, 0, NULL,
			&dummy_handler, def);
	vle_opts_add("path", "pt", "descr", OPT_STR, OPT_LOCAL, 0, NULL,
			&dummy_handler, def);

	saved_cwd = save_cwd();
	assert_success(chdir(TEST_DATA_PATH "/compare"));
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "compare", saved_cwd);

	curr_stats.cs = &cfg.cs;
}

TEARDOWN()
{
	cs_reset(&cfg.cs);
	curr_stats.cs = NULL;

	restore_cwd(saved_cwd);

	update_string(&cfg.slow_fs_list, NULL);

	free(stats.line);
	vle_opts_reset();

	function_reset_all();
}

TEST(test_set_completion)
{
	ASSERT_COMPLETION(L"set ", L"set all");
}

TEST(root_entries_are_completed)
{
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "", saved_cwd);
	assert_success(chdir(curr_view->curr_dir));

	prepare_for_line_completion(L"cd /");
	assert_success(line_completion(&stats));
	assert_true(wcscmp(L"cd /", stats.line) != 0);
}

TEST(dirs_are_completed_with_trailing_slash)
{
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "", saved_cwd);
	assert_success(chdir(curr_view->curr_dir));

	ASSERT_COMPLETION(L"cd r", L"cd read/");
	ASSERT_NEXT_MATCH("rename/");
	ASSERT_NEXT_MATCH("r");
	ASSERT_NEXT_MATCH("read/");
}

TEST(tabnew_has_directory_only_completion)
{
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "tree/dir1", saved_cwd);
	assert_success(chdir(curr_view->curr_dir));

	ASSERT_COMPLETION(L"tabnew ", L"tabnew dir2/");
	ASSERT_NEXT_MATCH("dir2/");
	ASSERT_NEXT_MATCH("dir2/");
}

TEST(function_name_completion)
{
	ASSERT_COMPLETION(L"echo e", L"echo executable(");
	ASSERT_NEXT_MATCH("expand(");
	ASSERT_NEXT_MATCH("extcached(");
	ASSERT_NEXT_MATCH("e");
}

TEST(abbreviations)
{
	vle_abbr_reset();
	assert_success(vle_abbr_add(L"lhs", L"rhs"));

	ASSERT_COMPLETION(L"cabbrev l", L"cabbrev lhs");
	ASSERT_COMPLETION(L"cnoreabbrev l", L"cnoreabbrev lhs");
	ASSERT_COMPLETION(L"cunabbrev l", L"cunabbrev lhs");
	ASSERT_NO_COMPLETION(L"cabbrev l l");

	vle_abbr_reset();
}

TEST(bang_exec_completion)
{
	char *const original_path_env = strdup(env_get("PATH"));

	restore_cwd(saved_cwd);
	assert_success(chdir(SANDBOX_PATH));
	saved_cwd = save_cwd();

	env_set("PATH", saved_cwd);
	update_path_env(1);

	create_executable("exec-for-completion" EXE_SUFFIX);

	ASSERT_COMPLETION(L"!exec-for-com", L"!exec-for-completion" EXE_SUFFIXW);

	assert_success(unlink("exec-for-completion" EXE_SUFFIX));

	env_set("PATH", original_path_env);
	update_path_env(1);
	free(original_path_env);
}

TEST(bang_abs_path_completion)
{
	wchar_t input[PATH_MAX + 1];
	wchar_t cmd[PATH_MAX + 1];
	char cwd[PATH_MAX + 1];
	wchar_t *wcwd;

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	assert_true(get_cwd(cwd, sizeof(cwd)) == cwd);
	wcwd = to_wide(cwd);

	create_executable("exec-for-completion" EXE_SUFFIX);

	vifm_swprintf(input, ARRAY_LEN(input),
			L"!%" WPRINTF_WSTR L"/exec-for-compl", wcwd);
	vifm_swprintf(cmd, ARRAY_LEN(cmd),
			L"!%" WPRINTF_WSTR L"/exec-for-completion" EXE_SUFFIXW, wcwd);

	ASSERT_COMPLETION(input, cmd);

	assert_int_equal(2, vle_compl_get_count());

	assert_success(unlink("exec-for-completion" EXE_SUFFIX));

	free(wcwd);
}

TEST(standalone_tilde_is_expanded)
{
	make_abs_path(cfg.home_dir, sizeof(cfg.home_dir), TEST_DATA_PATH, "/",
			saved_cwd);

	char *expected = format_str("cd %s", cfg.home_dir);
	wchar_t *wexpected = to_wide(expected);

	ASSERT_COMPLETION(L"cd ~", wexpected);

	free(wexpected);
	free(expected);
}

TEST(tilde_is_completed_after_emark)
{
	make_abs_path(cfg.home_dir, sizeof(cfg.home_dir), TEST_DATA_PATH, "/",
			saved_cwd);
	ASSERT_COMPLETION(L"!~/", L"!~/color-schemes/");
}

TEST(user_name_is_completed_after_tilde, IF(not_windows))
{
	prepare_for_line_completion(L"cd ~roo");
	assert_success(line_completion(&stats));
	char *narrow = to_multibyte(stats.line);
	assert_string_equal(NULL, strstr(narrow, "\\~"));
	free(narrow);
}

TEST(bmark_tags_are_completed)
{
	bmarks_clear();

	assert_success(cmds_dispatch("bmark! fake/path1 tag1", &lwin, CIT_COMMAND));

	ASSERT_COMPLETION(L"bmark tag", L"bmark tag1");
	ASSERT_COMPLETION(L"bmark! fake/path2 tag", L"bmark! fake/path2 tag1");
	ASSERT_NO_COMPLETION(L"bmark! fake/path2 ../");
	ASSERT_COMPLETION(L"bmark! fake/path2 ", L"bmark! fake/path2 tag1");
}

TEST(bmark_path_is_completed)
{
	bmarks_clear();

	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir), SANDBOX_PATH,
			"", saved_cwd);
	assert_success(chdir(curr_view->curr_dir));
	create_executable("exec-for-completion" EXE_SUFFIX);

	ASSERT_COMPLETION(L"bmark! exec", L"bmark! exec-for-completion" EXE_SUFFIX);

	assert_success(unlink("exec-for-completion" EXE_SUFFIX));
}

TEST(delbmark_tags_are_completed)
{
	bmarks_clear();

	assert_success(cmds_dispatch("bmark! fake/path1 tag1", &lwin, CIT_COMMAND));

	ASSERT_COMPLETION(L"delbmark ta", L"delbmark tag1");
}

TEST(selective_sync_completion)
{
	ASSERT_COMPLETION(L"sync! a", L"sync! all");
	ASSERT_NO_COMPLETION(L"sync! ../");
}

TEST(colorscheme_completion)
{
	make_abs_path(cfg.colors_dir, sizeof(cfg.colors_dir), TEST_DATA_PATH,
			"scripts", saved_cwd);
	ASSERT_COMPLETION(L"colorscheme set-", L"colorscheme set-env");
	ASSERT_COMPLETION(L"colorscheme set-env ../",
			L"colorscheme set-env ../color-schemes/");
	ASSERT_NO_COMPLETION(L"colorscheme ../");

	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "", saved_cwd);
	ASSERT_COMPLETION(L"colorscheme set-env ",
			L"colorscheme set-env color-schemes/");
}

TEST(wincmd_completion)
{
	ASSERT_COMPLETION(L"wincmd ", L"wincmd +");
	ASSERT_NO_COMPLETION(L"wincmd + ");
}

TEST(grep_completion)
{
	ASSERT_NO_COMPLETION(L"grep -");
	ASSERT_NO_COMPLETION(L"grep .");
	ASSERT_COMPLETION(L"grep -o ..", L"grep -o ../");

	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "", saved_cwd);
	assert_success(chdir(curr_view->curr_dir));

	ASSERT_COMPLETION(L"grep -o ", L"grep -o color-schemes/");
}

TEST(find_completion)
{
#ifdef _WIN32
	/* Windows escaping code doesn't prepend "./". */
	ASSERT_NO_COMPLETION(L"find -");
#else
	ASSERT_COMPLETION(L"find -", L"find ./-");
#endif

	ASSERT_COMPLETION(L"find ..", L"find ../");
	ASSERT_NO_COMPLETION(L"find . .");

	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "", saved_cwd);
	assert_success(chdir(curr_view->curr_dir));

	ASSERT_COMPLETION(L"find ", L"find color-schemes/");
}

TEST(aucmd_events_are_completed)
{
	ASSERT_COMPLETION(L"autocmd ", L"autocmd DirEnter");
	ASSERT_COMPLETION(L"autocmd Dir", L"autocmd DirEnter");
	ASSERT_COMPLETION(L"autocmd! Dir", L"autocmd! DirEnter");
	ASSERT_NO_COMPLETION(L"autocmd DirEnter ");
}

TEST(prefixless_option_name_is_completed)
{
	ASSERT_COMPLETION(L"echo &", L"echo &fusehome");
	assert_success(line_completion(&stats));
	assert_wstring_equal(L"echo &path", stats.line);
}

TEST(prefixed_global_option_name_is_completed)
{
	ASSERT_COMPLETION(L"echo &g:f", L"echo &g:fusehome");
}

TEST(prefixed_local_option_name_is_completed)
{
	ASSERT_COMPLETION(L"echo &l:p", L"echo &l:path");
}

TEST(autocmd_name_completion_is_case_insensitive)
{
	ASSERT_COMPLETION(L"autocmd dir", L"autocmd DirEnter");
}

TEST(case_override_of_paths)
{
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "existing-files", saved_cwd);
	assert_success(chdir(curr_view->curr_dir));

	cfg.ignore_case = 0;
	cfg.case_override = CO_PATH_COMPL;
	cfg.case_ignore = CO_PATH_COMPL;

	ASSERT_COMPLETION(L"edit A", L"edit a");

	cfg.case_override = 0;
	cfg.case_ignore = 0;
}

TEST(envvars_are_completed_for_edit)
{
	env_set("RRRRRARE_VARIABLE1", "1");
	env_set("RRRRRARE_VARIABLE2", "2");

	ASSERT_COMPLETION(L"edit $RRRRRARE_VARIA", L"edit $RRRRRARE_VARIABLE1");
}

TEST(builtinvars_are_completed_for_echo)
{
	init_variables();
	assert_success(setvar("v:test", var_from_bool(1)));
	ASSERT_COMPLETION(L"echo v:t", L"echo v:test");
	clear_variables();
}

TEST(select_is_completed)
{
	env_set("RRRRRARE_VARIABLE1", "1");
	env_set("RRRRRARE_VARIABLE2", "2");

	ASSERT_NO_COMPLETION(L"select $RRRRRARE_VARIA");
	ASSERT_NO_COMPLETION(L"select !/$RRRRRARE_VARIA");
	ASSERT_NO_COMPLETION(L"select !cmd some-arg");

	/* Check that not memory violations occur here. */
	prepare_for_line_completion(L"select !cmd ");
	assert_success(line_completion(&stats));

	ASSERT_COMPLETION(L"select!!$RRRRRARE_VARIA", L"select!!$RRRRRARE_VARIABLE1");
	ASSERT_COMPLETION(L"unselect !cat $RRRRRARE_VARIA",
			L"unselect !cat $RRRRRARE_VARIABLE1");
}

TEST(compare_is_completed)
{
	ASSERT_COMPLETION(L"compare by", L"compare bycontents");
	ASSERT_COMPLETION(L"compare bysize list", L"compare bysize listall");
}

TEST(symlinks_in_paths_are_not_resolved, IF(not_windows))
{
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	char src[PATH_MAX + 1], dst[PATH_MAX + 1];
	make_abs_path(src, sizeof(src), TEST_DATA_PATH, "compare", saved_cwd);
	make_abs_path(dst, sizeof(dst), SANDBOX_PATH, "dir-link", saved_cwd);
	assert_success(make_symlink(src, dst));

	assert_success(chdir(SANDBOX_PATH "/dir-link"));
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir), SANDBOX_PATH,
			"dir-link", saved_cwd);

	ASSERT_COMPLETION(L"cd ../d", L"cd ../dir-link/");

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();
	assert_success(remove(SANDBOX_PATH "/dir-link"));
}

TEST(session_is_completed)
{
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "",
			saved_cwd);

	create_dir(SANDBOX_PATH "/sessions");
	create_dir(SANDBOX_PATH "/sessions/subdir.json");
	create_file(SANDBOX_PATH "/sessions/subdir.json/file.json");
	create_file(SANDBOX_PATH "/sessions/session-a.json");
	create_file(SANDBOX_PATH "/sessions/session-b.json");

	ASSERT_COMPLETION(L"session ", L"session session-a");
	ASSERT_NEXT_MATCH("session-b");
	ASSERT_NEXT_MATCH("");

	ASSERT_NO_COMPLETION(L"session session-a ..");

	ASSERT_NO_COMPLETION(L"session! ses");
	ASSERT_NO_COMPLETION(L"session? ses");

	remove_file(SANDBOX_PATH "/sessions/session-b.json");
	remove_file(SANDBOX_PATH "/sessions/session-a.json");
	remove_file(SANDBOX_PATH "/sessions/subdir.json/file.json");
	remove_dir(SANDBOX_PATH "/sessions/subdir.json");
	remove_dir(SANDBOX_PATH "/sessions");

	cfg.config_dir[0] = '\0';
}

TEST(delsession_is_completed)
{
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "",
			saved_cwd);

	create_dir(SANDBOX_PATH "/sessions");
	create_dir(SANDBOX_PATH "/sessions/subdir.json");
	create_file(SANDBOX_PATH "/sessions/subdir.json/file.json");
	create_file(SANDBOX_PATH "/sessions/session-a.json");
	create_file(SANDBOX_PATH "/sessions/session-b.json");

	ASSERT_COMPLETION(L"delsession ", L"delsession session-a");
	ASSERT_NEXT_MATCH("session-b");
	ASSERT_NEXT_MATCH("");

	ASSERT_NO_COMPLETION(L"delsession session-a ..");

	ASSERT_NO_COMPLETION(L"delsession! ses");
	ASSERT_NO_COMPLETION(L"delsession? ses");

	remove_file(SANDBOX_PATH "/sessions/session-b.json");
	remove_file(SANDBOX_PATH "/sessions/session-a.json");
	remove_file(SANDBOX_PATH "/sessions/subdir.json/file.json");
	remove_dir(SANDBOX_PATH "/sessions/subdir.json");
	remove_dir(SANDBOX_PATH "/sessions");

	cfg.config_dir[0] = '\0';
}

TEST(plugin_is_completed)
{
	ASSERT_COMPLETION(L"plugin ", L"plugin blacklist");
	ASSERT_NEXT_MATCH("load");
	ASSERT_NEXT_MATCH("whitelist");
	ASSERT_NEXT_MATCH("");

	ASSERT_COMPLETION(L"plugin w", L"plugin whitelist");
	ASSERT_NEXT_MATCH("whitelist");

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", NULL);
	create_dir(SANDBOX_PATH "/plugins");
	create_dir(SANDBOX_PATH "/plugins/plug1");
	create_dir(SANDBOX_PATH "/plugins/plug2");
	make_file(SANDBOX_PATH "/plugins/plug1/init.lua", "return {}");
	make_file(SANDBOX_PATH "/plugins/plug2/init.lua", "return");

	curr_stats.vlua = vlua_init();
	curr_stats.plugs = plugs_create(curr_stats.vlua);
	load_plugins(curr_stats.plugs, cfg.config_dir);

	ASSERT_COMPLETION(L"plugin whitelist ", L"plugin whitelist plug1");
	ASSERT_NEXT_MATCH("plug2");
	ASSERT_NEXT_MATCH("");

	plugs_free(curr_stats.plugs);
	vlua_finish(curr_stats.vlua);
	curr_stats.plugs = NULL;
	curr_stats.vlua = NULL;

	remove_file(SANDBOX_PATH "/plugins/plug1/init.lua");
	remove_file(SANDBOX_PATH "/plugins/plug2/init.lua");
	remove_dir(SANDBOX_PATH "/plugins/plug1");
	remove_dir(SANDBOX_PATH "/plugins/plug2");
	remove_dir(SANDBOX_PATH "/plugins");
}

TEST(tree_is_completed)
{
	prepare_for_line_completion(L"tree ");

	assert_success(line_completion(&stats));
	assert_wstring_equal(L"tree depth=", stats.line);
}

TEST(highlight_is_completed)
{
	ASSERT_COMPLETION(L"hi ", L"hi AuxWin");
	ASSERT_COMPLETION(L"hi wi", L"hi WildMenu");
	ASSERT_COMPLETION(L"hi WildMenu cter", L"hi WildMenu cterm");

	ASSERT_COMPLETION(L"hi WildMenu ctermfg=def", L"hi WildMenu ctermfg=default");
	ASSERT_COMPLETION(L"hi WildMenu ctermfg=no", L"hi WildMenu ctermfg=none");
	ASSERT_COMPLETION(L"hi WildMenu ctermfg=r", L"hi WildMenu ctermfg=Red1");
	ASSERT_COMPLETION(L"hi WildMenu ctermfg=lightb",
			L"hi WildMenu ctermfg=lightblack");
	ASSERT_COMPLETION(L"hi WildMenu cterm=re", L"hi WildMenu cterm=reverse");
	ASSERT_COMPLETION(L"hi WildMenu cterm=bold,re",
			L"hi WildMenu cterm=bold,reverse");

	ASSERT_COMPLETION(L"hi WildMenu guibg=r", L"hi WildMenu guibg=red");
	ASSERT_COMPLETION(L"hi WildMenu guibg=l", L"hi WildMenu guibg=l");

	assert_success(cmds_dispatch("hi {*.jpg} cterm=none", &lwin, CIT_COMMAND));
	ASSERT_COMPLETION(L"hi clear ", L"hi clear {*.jpg}");
}

TEST(command_options_are_completed)
{
	ASSERT_COMPLETION(L"copy -", L"copy -skip");

	other_view = &rwin;
#ifndef _WIN32
	ASSERT_COMPLETION(L"copy -- -", L"copy -- ./-");
#else
	ASSERT_COMPLETION(L"copy -- -", L"copy -- -");
#endif
	other_view = NULL;
}

static void
dummy_handler(OPT_OP op, optval_t val)
{
	ASSERT_NO_COMPLETION(L"nosuchcommand a");
}

static void
prepare_for_line_completion(const wchar_t str[])
{
	free(stats.line);
	stats.line = wcsdup(str);
	stats.len = wcslen(stats.line);
	stats.index = stats.len;
	stats.complete_continue = 0;

	vle_compl_reset();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
