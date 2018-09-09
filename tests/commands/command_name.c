#include <stic.h>

#include <string.h>

#include "../../src/engine/cmds.h"
#include "../../src/utils/macros.h"

static int goto_cmd(const cmd_info_t *cmd_info);
static int delete_cmd(const cmd_info_t *cmd_info);
static int shell_cmd(const cmd_info_t *cmd_info);
static int substitute_cmd(const cmd_info_t *cmd_info);

extern cmds_conf_t cmds_conf;

static cmd_info_t cmdi;
static int shell_called;
static int substitute_called;

static const cmd_add_t commands[] = {
	{ .name = "",             .abbr = NULL,  .id = -1,      .descr = "descr",
	  .flags = HAS_RANGE,
	  .handler = &goto_cmd,   .min_args = 0, .max_args = 0, },
	{ .name = "delete",       .abbr = "d",   .id = -1,      .descr = "descr",
	  .flags = HAS_EMARK | HAS_RANGE,
	  .handler = &delete_cmd, .min_args = 0, .max_args = 1, },
	{ .name = "shell",        .abbr = "sh",  .id = -1,      .descr = "descr",
	  .flags = HAS_EMARK | HAS_QMARK_WITH_ARGS,
	  .handler = &shell_cmd,  .min_args = 0, .max_args = 1, },
	{ .name = "substitute",   .abbr = "s",   .id = -1,      .descr = "descr",
	  .flags = HAS_REGEXP_ARGS | HAS_CUST_SEP,
	  .handler = &substitute_cmd, .min_args = 0, .max_args = 3, },
};

static int
goto_cmd(const cmd_info_t *cmd_info)
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
	add_builtin_commands(commands, ARRAY_LEN(commands));
}

TEST(builtin)
{
	cmd_add_t command = {
	  .name = "",           .abbr = NULL,  .id = -1,      .descr = "descr",
	  .flags = HAS_RANGE,
	  .handler = &goto_cmd, .min_args = 0, .max_args = 0,
	};

	assert_true(add_builtin_cmd("!", 0, &command) == 0);

	assert_false(add_builtin_cmd("?", 0, &command) == 0);

	assert_false(add_builtin_cmd("&", 0, &command) == 0);

	assert_false(add_builtin_cmd("2", 0, &command) == 0);
}

TEST(user_add)
{
	assert_int_equal(0, execute_cmd("command comix a"));

	assert_int_equal(0, execute_cmd("command udf a"));

	assert_int_equal(CMDS_ERR_INCORRECT_NAME, execute_cmd("command ! a"));

	assert_int_equal(0, execute_cmd("command udf! a"));

	assert_int_equal(CMDS_ERR_INCORRECT_NAME, execute_cmd("command ud!f a"));

	assert_int_equal(0, execute_cmd("command udf? a"));

	assert_int_equal(CMDS_ERR_INCORRECT_NAME, execute_cmd("command u?df a"));

	assert_int_equal(CMDS_ERR_INCORRECT_NAME, execute_cmd("command u0d1f2 a"));

	assert_int_equal(CMDS_ERR_INCORRECT_NAME, execute_cmd("command 0u0d1f2 a"));
}

TEST(user_exec)
{
	assert_int_equal(0, execute_cmd("command udf a"));
	assert_int_equal(0, execute_cmd("command udf! a"));
	assert_int_equal(0, execute_cmd("command udf? a"));

	assert_int_equal(0, execute_cmd("udf"));
	assert_int_equal(0, execute_cmd("udf!"));
	assert_int_equal(0, execute_cmd("udf?"));
}

TEST(abbreviations)
{
	assert_int_equal(0, execute_cmd("%d!"));
	assert_int_equal(10, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);
}

TEST(udf_bang_abbr)
{
	assert_int_equal(0, execute_cmd("command udf! a"));
	assert_int_equal(0, execute_cmd("command UDF? a"));

	assert_int_equal(0, execute_cmd("udf!"));
	assert_int_equal(0, execute_cmd("ud!"));
	assert_int_equal(0, execute_cmd("u!"));

	assert_int_equal(0, execute_cmd("UDF?"));
	assert_int_equal(0, execute_cmd("UD?"));
	assert_int_equal(0, execute_cmd("U?"));
}

TEST(cust_sep_is_resolved_correctly)
{
	shell_called = substitute_called = 0;
	assert_success(execute_cmd("s!a!b!g"));
	assert_false(shell_called);
	assert_true(substitute_called);

	shell_called = substitute_called = 0;
	assert_success(execute_cmd("s?a?b?g"));
	assert_false(shell_called);
	assert_true(substitute_called);

	shell_called = substitute_called = 0;
	assert_success(execute_cmd("s:a:b:g"));
	assert_false(shell_called);
	assert_true(substitute_called);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
