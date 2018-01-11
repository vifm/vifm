#include <stic.h>

#include <unistd.h> /* chdir() rmdir() */

#include <stdio.h> /* remove() */

#include "../../src/compat/os.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/macros.h"
#include "../../src/utils/path.h"
#include "../../src/args.h"
#include "../../src/status.h"

#include "utils.h"

static char *saved_cwd;

SETUP()
{
	saved_cwd = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	curr_stats.load_stage = 1;
}

TEARDOWN()
{
	restore_cwd(saved_cwd);

	curr_stats.load_stage = 0;
}

TEST(chooseopt_options_are_not_set)
{
	args_t args = { };
	char *argv[] = { "vifm", "+", NULL };

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");

	assert_int_equal(1, args.ncmds);
	assert_string_equal("$", args.cmds[0]);

	args_free(&args);
}

TEST(dash_is_accepted)
{
	args_t args = { };
	char *argv[] = { "vifm", "-", NULL };

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");

	assert_string_equal("-", args.lwin_path);

	args_free(&args);
}

TEST(select_does_not_accept_dash_if_no_such_file)
{
	args_t args = { .lwin_handle = 135 };
	char *argv[] = { "vifm", "--select", "-", NULL };

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");

	assert_string_equal("", args.lwin_path);
	/* XXX: this is a hack to postpone updating args.c unit. */
	assert_int_equal(135, args.lwin_handle);

	args_free(&args);
}

TEST(select_accepts_dash_if_such_file_exists)
{
	args_t args = { };
	char *argv[] = { "vifm", "--select", "-", NULL };

	create_file("-");

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");

	assert_string_equal("/-", args.lwin_path);

	assert_success(remove("-"));
	args_free(&args);
}

TEST(select_accepts_dash_if_such_directory_exists)
{
	args_t args = { };
	char *argv[] = { "vifm", "--select", "-", NULL };

	assert_success(os_mkdir("-", 0700));

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");

	assert_string_equal("/-", args.lwin_path);

	assert_success(rmdir("-"));
	args_free(&args);
}

TEST(remote_allows_no_arguments)
{
	args_t args = { };
	char *argv[] = { "vifm", "a", "--remote", NULL };

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");
	assert_string_equal(NULL, args.remote_cmds[0]);
	args_free(&args);
}

TEST(remote_takes_all_arguments_to_the_right)
{
	args_t args = { };
	char *argv[] = { "vifm", "a", "--remote", "b", "c", NULL };

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");

	assert_string_equal("b", args.remote_cmds[0]);
	assert_string_equal("c", args.remote_cmds[1]);
	assert_string_equal(NULL, args.remote_cmds[2]);

	args_free(&args);
}

TEST(remote_expr_is_parsed)
{
	args_t args = { };
	char *argv[] = { "vifm", "--remote-expr", "expr", NULL };

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");
	assert_string_equal("expr", args.remote_expr);
	args_free(&args);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
