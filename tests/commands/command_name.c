#include <stic.h>

#include <string.h>

#include "../../src/engine/cmds.h"
#include "../../src/utils/macros.h"

static int goto_cmd(const cmd_info_t *cmd_info);
static int delete_cmd(const cmd_info_t *cmd_info);

extern cmds_conf_t cmds_conf;

static cmd_info_t cmdi;

static const cmd_add_t commands[] = {
	{ .name = "",             .abbr = NULL,  .id = -1,      .descr = "descr",
	  .flags = HAS_RANGE,
	  .handler = &goto_cmd,   .min_args = 0, .max_args = 0, },
	{ .name = "delete",       .abbr = "d",   .id = -1,      .descr = "descr",
	  .flags = HAS_EMARK | HAS_RANGE,
	  .handler = &delete_cmd, .min_args = 0, .max_args = 1, },
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
