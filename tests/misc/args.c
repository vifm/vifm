#include <stic.h>

#include <unistd.h> /* chdir() rmdir() */

#include <stdio.h> /* remove() */

#include <test-utils.h>

#include "../../src/compat/os.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/macros.h"
#include "../../src/utils/path.h"
#include "../../src/args.h"
#include "../../src/status.h"

static int with_remote_cmds(void);

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

TEST(empty_cmd)
{
	args_t args = { };
	char *argv[] = { "vifm", "+", NULL };

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");

	assert_int_equal(1, args.ncmds);
	assert_string_equal("$", args.cmds[0]);

	args_free(&args);
}

TEST(cmds_add_up)
{
	args_t args = { };
	char *argv[] = { "vifm", "-ccmd0", "+cmd1", "-c", "cmd2", NULL };

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");

	assert_int_equal(3, args.ncmds);
	assert_string_equal("cmd0", args.cmds[0]);
	assert_string_equal("cmd1", args.cmds[1]);
	assert_string_equal("cmd2", args.cmds[2]);

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

TEST(can_output_choice_on_stdout)
{
	args_t args = { };
	char *argv[] = { "vifm", "--choose-dir", "-", "--choose-file", "-", NULL };

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");
	assert_string_equal("-", args.chosen_files_out);
	assert_string_equal("-", args.chosen_dir_out);
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

TEST(it_is_either_help_or_version_not_both)
{
	args_t help_args = { };
	char *help_argv[] = { "vifm", "--help", "--version", NULL };
	args_parse(&help_args, ARRAY_LEN(help_argv) - 1U, help_argv, "/");
	assert_true(help_args.help);
	assert_false(help_args.version);
	args_free(&help_args);

	args_t version_args = { };
	char *version_argv[] = { "vifm", "--version", "--help", NULL };
	args_parse(&version_args, ARRAY_LEN(version_argv) - 1U, version_argv, "/");
	assert_false(version_args.help);
	assert_true(version_args.version);
	args_free(&version_args);
}

TEST(logging_without_arg)
{
	args_t args = { };
	char *argv[] = { "vifm", "--logging", NULL };

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");
	assert_true(args.logging);
	assert_string_equal(NULL, args.startup_log_path);
	args_free(&args);
}

TEST(logging_with_arg)
{
	args_t args = { };
	char *argv[] = { "vifm", "--logging=startup-log", NULL };

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");
	assert_true(args.logging);
	assert_string_equal("startup-log", args.startup_log_path);
	args_free(&args);
}

TEST(various_flags)
{
	args_t args = { };
	char *argv[] = { "vifm", "-f", "--no-configs", NULL };

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");
	assert_true(args.file_picker);
	assert_true(args.no_configs);
	args_free(&args);
}

TEST(remote_allows_no_arguments, IF(with_remote_cmds))
{
	args_t args = { };
	char *argv[] = { "vifm", "a", "--remote", NULL };

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");
	assert_string_equal(NULL, args.remote_cmds[0]);
	args_free(&args);
}

TEST(remote_takes_all_arguments_to_the_right, IF(with_remote_cmds))
{
	args_t args = { };
	char *argv[] = { "vifm", "a", "--remote", "b", "c", NULL };

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");

	assert_string_equal("b", args.remote_cmds[0]);
	assert_string_equal("c", args.remote_cmds[1]);
	assert_string_equal(NULL, args.remote_cmds[2]);

	args_free(&args);
}

TEST(remote_expr_is_parsed, IF(with_remote_cmds))
{
	args_t args = { };
	char *argv[] = { "vifm", "--remote-expr", "expr", NULL };

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");
	assert_string_equal("expr", args.remote_expr);
	args_free(&args);
}

TEST(server_name_becomes_target_name, IF(with_remote_cmds))
{
	args_t args = { };
	char *argv[] = { "vifm", "--server-name", "name", "--remote", "path", NULL };

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");
	assert_string_equal(NULL, args.server_name);
	assert_string_equal("name", args.target_name);
	args_free(&args);
}

static int
with_remote_cmds(void)
{
#ifdef ENABLE_REMOTE_CMDS
	return 1;
#else
	return 0;
#endif
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
