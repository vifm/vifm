#include <stic.h>

#include <test-utils.h>

#include <locale.h> /* LC_ALL setlocale() */
#include <wchar.h> /* wcsdup() wcslen() */

#include "../../src/cfg/config.h"
#include "../../src/engine/completion.h"
#include "../../src/engine/cmds.h"
#include "../../src/compat/os.h"
#include "../../src/modes/cmdline.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"

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

static int dquotes_allowed_in_paths(void);
static void prepare_for_line_completion(const wchar_t str[]);

static line_stats_t stats;
static char *saved_cwd;

SETUP()
{
	stats.line = wcsdup(L"");
	stats.index = wcslen(stats.line);
	stats.curs_pos = 0;
	stats.len = stats.index;
	stats.cmd_pos = -1;
	stats.complete_continue = 0;
	stats.history_search = 0;
	stats.line_buf = NULL;
	stats.complete = &vle_cmds_complete;

	curr_view = &lwin;

	init_commands();

	vle_cmds_run("command bar a");
	vle_cmds_run("command baz b");
	vle_cmds_run("command foo c");

	saved_cwd = save_cwd();
	assert_success(chdir(TEST_DATA_PATH "/compare"));
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "compare", saved_cwd);
}

TEARDOWN()
{
	restore_cwd(saved_cwd);

	free(stats.line);
	vle_cmds_reset();
}

TEST(vim_like_completion)
{
	vle_compl_reset();
	assert_int_equal(0, vle_cmds_complete("e", NULL));
	ASSERT_NEXT_MATCH("echo");
	ASSERT_NEXT_MATCH("edit");
	ASSERT_NEXT_MATCH("else");
	ASSERT_NEXT_MATCH("elseif");
	ASSERT_NEXT_MATCH("empty");
	ASSERT_NEXT_MATCH("endif");
	ASSERT_NEXT_MATCH("execute");
	ASSERT_NEXT_MATCH("exit");
	ASSERT_NEXT_MATCH("e");

	vle_compl_reset();
	assert_int_equal(0, vle_cmds_complete("vm", NULL));
	ASSERT_NEXT_MATCH("vmap");
	ASSERT_NEXT_MATCH("vmap");

	vle_compl_reset();
	assert_int_equal(0, vle_cmds_complete("j", NULL));
	ASSERT_NEXT_MATCH("jobs");
	ASSERT_NEXT_MATCH("jobs");
}

TEST(leave_spaces_at_begin)
{
	vle_compl_reset();
	assert_int_equal(1, vle_cmds_complete(" qui", NULL));
	ASSERT_NEXT_MATCH("quit");
	ASSERT_NEXT_MATCH("quit");
}

TEST(only_user)
{
	vle_compl_reset();
	assert_int_equal(8, vle_cmds_complete("command ", NULL));
	ASSERT_NEXT_MATCH("bar");

	vle_compl_reset();
	assert_int_equal(9, vle_cmds_complete(" command ", NULL));
	ASSERT_NEXT_MATCH("bar");

	vle_compl_reset();
	assert_int_equal(10, vle_cmds_complete("  command ", NULL));
	ASSERT_NEXT_MATCH("bar");
}

TEST(no_sdquoted_completion_does_nothing)
{
	ASSERT_NO_COMPLETION(L"command '");
}

TEST(spaces_escaping_leading)
{
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir), SANDBOX_PATH,
			"", saved_cwd);
	assert_success(chdir(saved_cwd));
	create_file(SANDBOX_PATH "/ begins-with-space");

	ASSERT_COMPLETION(L"touch \\ ", L"touch \\ begins-with-space");

	remove_file(SANDBOX_PATH "/ begins-with-space");
}

/* Windows doesn't allow file names with trailing spaces. */
TEST(spaces_escaping_everywhere, IF(not_windows))
{
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir), SANDBOX_PATH,
			"", saved_cwd);
	assert_success(chdir(saved_cwd));
	create_file(SANDBOX_PATH "/ spaces everywhere ");

	ASSERT_COMPLETION(L"touch \\ s", L"touch \\ spaces\\ everywhere\\ ");

	remove_file(SANDBOX_PATH "/ spaces everywhere ");
}

/* Windows doesn't allow file names with trailing spaces. */
TEST(spaces_escaping_trailing, IF(not_windows))
{
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir), SANDBOX_PATH,
			"", saved_cwd);
	assert_success(chdir(saved_cwd));
	create_file(SANDBOX_PATH "/ends-with-space ");

	ASSERT_COMPLETION(L"touch e", L"touch ends-with-space\\ ");

	remove_file(SANDBOX_PATH "/ends-with-space ");
}

TEST(spaces_escaping_middle)
{
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir), SANDBOX_PATH,
			"", saved_cwd);
	assert_success(chdir(saved_cwd));
	create_file(SANDBOX_PATH "/spaces in the middle");

	ASSERT_COMPLETION(L"touch s", L"touch spaces\\ in\\ the\\ middle");

	remove_file(SANDBOX_PATH "/spaces in the middle");
}

TEST(squoted_completion)
{
	ASSERT_COMPLETION(L"touch '", L"touch 'a");
}

TEST(squoted_completion_escaping)
{
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "quotes-in-names", saved_cwd);
	assert_success(chdir(curr_view->curr_dir));

	ASSERT_COMPLETION(L"touch 's-quote", L"touch 's-quote-''-in-name");
}

TEST(dquoted_completion)
{
	ASSERT_COMPLETION(L"touch \"", L"touch \"a");
}

TEST(dquoted_completion_escaping, IF(dquotes_allowed_in_paths))
{
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(chdir(SANDBOX_PATH));
	assert_true(get_cwd(curr_view->curr_dir, sizeof(curr_view->curr_dir)) ==
			curr_view->curr_dir);

	create_file("d-quote-\"-in-name");
	create_file("d-quote-\"-in-name-2");
	create_file("d-quote-\"-in-name-3");

	ASSERT_COMPLETION(L"touch \"d-quote", L"touch \"d-quote-\\\"-in-name");

	assert_success(unlink("d-quote-\"-in-name"));
	assert_success(unlink("d-quote-\"-in-name-2"));
	assert_success(unlink("d-quote-\"-in-name-3"));
}

TEST(last_match_is_properly_escaped)
{
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "quotes-in-names", saved_cwd);
	assert_success(chdir(curr_view->curr_dir));

	ASSERT_COMPLETION(L"touch 's-quote-''-in", L"touch 's-quote-''-in-name");
	ASSERT_NEXT_MATCH("s-quote-''-in-name-2");
	ASSERT_NEXT_MATCH("s-quote-''-in");
}

TEST(emark_cmd_escaping)
{
	ASSERT_COMPLETION(L"", L"!");
	ASSERT_NEXT_MATCH("alink");
}

TEST(winrun_cmd_escaping)
{
	ASSERT_COMPLETION(L"winrun ", L"winrun $");
	ASSERT_NEXT_MATCH("%");
	ASSERT_NEXT_MATCH(",");
	ASSERT_NEXT_MATCH(".");
	ASSERT_NEXT_MATCH("^");
}

TEST(help_cmd_escaping)
{
	cfg.use_vim_help = 1;
	ASSERT_COMPLETION(L"help vifm-", L"help vifm-!!");
}

TEST(percent_completion)
{
	/* One percent symbol. */

	ASSERT_COMPLETION(L"cd %", L"cd %%");
	ASSERT_NEXT_MATCH("%%");
	ASSERT_NEXT_MATCH("%%");

	/* Two percent symbols. */

	ASSERT_NO_COMPLETION(L"cd %%");
	ASSERT_NEXT_MATCH("%%");
	ASSERT_NEXT_MATCH("%%");

	/* Three percent symbols. */

	ASSERT_COMPLETION(L"cd %%%", L"cd %%%%");
	ASSERT_NEXT_MATCH("%%%%");
	ASSERT_NEXT_MATCH("%%%%");
}

TEST(failure_to_turn_input_into_multibyte_is_handled, IF(not_windows))
{
	/* On Windows narrow locale doesn't really matter because UTF-8 is used
	 * internally by vifm. */
	prepare_for_line_completion(L"абвгд | set  all");
	stats.index = 12;

	(void)setlocale(LC_ALL, "C");
	assert_failure(line_completion(&stats));
	try_enable_utf8_locale();
	assert_wstring_equal(L"абвгд | set  all", stats.line);
}

TEST(failure_to_turn_completion_into_multibyte_is_handled, IF(not_windows))
{
	/* On Windows narrow locale doesn't really matter because UTF-8 is used
	 * internally by vifm. */
	prepare_for_line_completion(L"");
	stats.complete_continue = 1;

	vle_compl_reset();
	vle_compl_add_match("match is абвгд", "descr");

	(void)setlocale(LC_ALL, "C");
	assert_success(line_completion(&stats));
	try_enable_utf8_locale();

	assert_true(wcsncmp(L"match is ", stats.line, 9) == 0);
}

TEST(non_latin_prefix_does_not_break_completion, IF(utf8_locale))
{
	prepare_for_line_completion(L"абвгд | set ");

	assert_success(line_completion(&stats));
	assert_wstring_equal(L"абвгд | set all", stats.line);
}

TEST(autocd_completion)
{
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir), SANDBOX_PATH,
			"", saved_cwd);

	cfg.auto_cd = 1;

	create_dir(SANDBOX_PATH "/mydir1");
	create_dir(SANDBOX_PATH "/mydir2");
	create_dir(SANDBOX_PATH "/mydir3");

	ASSERT_COMPLETION(L"myd", L"mydir1/");
	ASSERT_NEXT_MATCH("mydir2/");
	ASSERT_NEXT_MATCH("mydir3/");
	ASSERT_NEXT_MATCH("myd");

	ASSERT_COMPLETION(L"../m", L"../misc/");
	ASSERT_COMPLETION(L"../misc/my", L"../misc/mydir1/");

	cfg.auto_cd = 0;

	ASSERT_NO_COMPLETION(L"myd");

	remove_dir(SANDBOX_PATH "/mydir1");
	remove_dir(SANDBOX_PATH "/mydir2");
	remove_dir(SANDBOX_PATH "/mydir3");
}

static int
dquotes_allowed_in_paths(void)
{
	if(os_mkdir(SANDBOX_PATH "/a\"b", 0700) == 0)
	{
		assert_success(rmdir(SANDBOX_PATH "/a\"b"));
		return 1;
	}
	return 0;
}

static void
prepare_for_line_completion(const wchar_t str[])
{
	free(stats.line);
	stats.line = wcsdup(str);
	stats.len = wcslen(stats.line);
	stats.index = stats.len;
	stats.complete_continue = 0;
	stats.prefix_len = 0;

	vle_compl_reset();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
