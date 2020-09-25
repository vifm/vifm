#include <stic.h>

#include <string.h>

#include "../../src/engine/cmds.h"
#include "../../src/utils/macros.h"

#include "suite.h"

static int dummy_cmd(const cmd_info_t *cmd_info);
static int delete_cmd(const cmd_info_t *cmd_info);
static int shell_cmd(const cmd_info_t *cmd_info);
static int substitute_cmd(const cmd_info_t *cmd_info);
static int usercmd_cmd(const cmd_info_t *cmd_info);

extern cmds_conf_t cmds_conf;

static cmd_info_t cmdi;
static int shell_called;
static int substitute_called;

static const cmd_add_t commands[] = {
	{ .name = "",             .abbr = NULL,  .id = -1,      .descr = "descr",
	  .flags = HAS_RANGE,
	  .handler = &dummy_cmd,  .min_args = 0, .max_args = 0, },
	{ .name = "delete",       .abbr = "d",   .id = -1,      .descr = "descr",
	  .flags = HAS_EMARK | HAS_RANGE,
	  .handler = &delete_cmd, .min_args = 0, .max_args = 1, },
	{ .name = "shell",        .abbr = "sh",  .id = -1,      .descr = "descr",
	  .flags = HAS_EMARK | HAS_QMARK_WITH_ARGS,
	  .handler = &shell_cmd,  .min_args = 0, .max_args = 1, },
	{ .name = "substitute",   .abbr = "s",   .id = -1,      .descr = "descr",
	  .flags = HAS_REGEXP_ARGS | HAS_CUST_SEP,
	  .handler = &substitute_cmd, .min_args = 0, .max_args = 3, },
	{ .name = "tr",           .abbr = NULL,  .id = -1,      .descr = "descr",
	  .flags = HAS_REGEXP_ARGS | HAS_CUST_SEP,
	  .handler = &dummy_cmd,  .min_args = 2, .max_args = 2, },
};

static int
dummy_cmd(const cmd_info_t *cmd_info)
{
	return 0;
}

static int
delete_cmd(const cmd_info_t *cmd_info)
{
	cmdi = *cmd_info;
	return 0;
}

static int
shell_cmd(const cmd_info_t *cmd_info)
{
	shell_called = 1;
	return 0;
}

static int
substitute_cmd(const cmd_info_t *cmd_info)
{
	substitute_called = 1;
	return 0;
}

SETUP()
{
	vle_cmds_add(commands, ARRAY_LEN(commands));
}

TEST(builtin)
{
	cmd_add_t command = {
	  .name = "",           .abbr = NULL,  .id = -1,      .descr = "descr",
	  .flags = HAS_RANGE,
	  .handler = &dummy_cmd, .min_args = 0, .max_args = 0,
	};

	assert_true(add_builtin_cmd("!", BUILTIN_CMD, &command) == 0);

	assert_false(add_builtin_cmd("?", BUILTIN_CMD, &command) == 0);

	assert_false(add_builtin_cmd("&", BUILTIN_CMD, &command) == 0);

	assert_false(add_builtin_cmd("2", BUILTIN_CMD, &command) == 0);
}

TEST(user_add)
{
	assert_int_equal(0, vle_cmds_run("command comix a"));

	assert_int_equal(0, vle_cmds_run("command udf a"));

	assert_int_equal(CMDS_ERR_INCORRECT_NAME, vle_cmds_run("command ! a"));
	assert_int_equal(CMDS_ERR_INCORRECT_NAME, vle_cmds_run("command ? a"));

	assert_int_equal(CMDS_ERR_INCORRECT_NAME, vle_cmds_run("command a# a"));

	assert_int_equal(0, vle_cmds_run("command udf! a"));

	assert_int_equal(CMDS_ERR_INCORRECT_NAME, vle_cmds_run("command ud!f a"));

	assert_int_equal(0, vle_cmds_run("command udf? a"));

	assert_int_equal(CMDS_ERR_INCORRECT_NAME, vle_cmds_run("command u?df a"));

	assert_int_equal(CMDS_ERR_INCORRECT_NAME, vle_cmds_run("command u0d1f2 a"));

	assert_int_equal(CMDS_ERR_INCORRECT_NAME, vle_cmds_run("command 0u0d1f2 a"));
}

TEST(user_defined_name_cannot_be_empty)
{
	assert_failure(vle_cmds_run("command '' a"));
}

TEST(user_exec)
{
	assert_int_equal(0, vle_cmds_run("command udf a"));
	assert_int_equal(0, vle_cmds_run("command udf! a"));
	assert_int_equal(0, vle_cmds_run("command udf? a"));

	assert_int_equal(0, vle_cmds_run("udf"));
	assert_int_equal(0, vle_cmds_run("udf!"));
	assert_int_equal(0, vle_cmds_run("udf?"));
}

TEST(abbreviations)
{
	assert_int_equal(0, vle_cmds_run("%d!"));
	assert_int_equal(10, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);
}

TEST(udf_bang_abbr)
{
	assert_int_equal(0, vle_cmds_run("command udf! a"));
	assert_int_equal(0, vle_cmds_run("command UDF? a"));

	assert_int_equal(0, vle_cmds_run("udf!"));
	assert_int_equal(0, vle_cmds_run("ud!"));
	assert_int_equal(0, vle_cmds_run("u!"));

	assert_int_equal(0, vle_cmds_run("UDF?"));
	assert_int_equal(0, vle_cmds_run("UD?"));
	assert_int_equal(0, vle_cmds_run("U?"));

	/* These will never be called due to :s accepting custom separator. */
	assert_failure(vle_cmds_run("command s! a"));
	assert_failure(vle_cmds_run("command s? a"));

	user_cmd_handler = &usercmd_cmd;

	substitute_called = 0;
	assert_success(vle_cmds_run("command t! s"));
	assert_success(vle_cmds_run("tr!1!3!"));
	assert_false(substitute_called);
	assert_success(vle_cmds_run("t!1!3!"));
	assert_true(substitute_called);
}

TEST(cust_sep_is_resolved_correctly)
{
	shell_called = substitute_called = 0;
	assert_success(vle_cmds_run("s!a!b!g"));
	assert_false(shell_called);
	assert_true(substitute_called);

	shell_called = substitute_called = 0;
	assert_success(vle_cmds_run("s?a?b?g"));
	assert_false(shell_called);
	assert_true(substitute_called);

	shell_called = substitute_called = 0;
	assert_success(vle_cmds_run("s:a:b:g"));
	assert_false(shell_called);
	assert_true(substitute_called);
}

TEST(cant_register_usercmd_twice)
{
	cmd_add_t command = {
	  .name = "<USERCMD>",   .abbr = NULL,  .id = -1,      .descr = "descr",
	  .flags = HAS_RANGE,
	  .handler = &dummy_cmd, .min_args = 0, .max_args = 0,
	};

	assert_failure(add_builtin_cmd("<USERCMD>", BUILTIN_CMD, &command));
}

TEST(cant_register_same_builtin_twice)
{
	cmd_add_t command = {
	  .name = "tr",          .abbr = NULL,  .id = -1,      .descr = "descr",
	  .flags = HAS_RANGE,
	  .handler = &dummy_cmd, .min_args = 0, .max_args = 0,
	};

	assert_failure(add_builtin_cmd("tr", BUILTIN_CMD, &command));
}

static int
usercmd_cmd(const cmd_info_t *cmd_info)
{
	return vle_cmds_run(cmd_info->user_action);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
