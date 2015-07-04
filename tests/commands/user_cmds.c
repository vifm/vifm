#include <stic.h>

#include <string.h>

#include "../../src/engine/cmds.h"

static int move_cmd(const cmd_info_t *cmd_info);

extern cmds_conf_t cmds_conf;

cmd_info_t user_cmd_info;

static int move_cmd_called;

SETUP()
{
	static const cmd_add_t move = {
		.name = "move",       .abbr = "m", .emark = 1,  .id = -1,      .range = 1,    .bg = 1, .quote = 1, .regexp = 0,
		.handler = &move_cmd, .qmark = 1,  .expand = 0, .cust_sep = 0, .min_args = 0, .max_args = NOT_DEF, .select = 1,
	};

	add_builtin_commands(&move, 1);

	assert_success(execute_cmd("command udf a"));
	assert_success(execute_cmd("command mkcd! a"));
	assert_success(execute_cmd("command mkcd? a"));
}

static int
move_cmd(const cmd_info_t *cmd_info)
{
	move_cmd_called = 1;
	return 0;
}

TEST(user_defined_command)
{
	cmds_conf.begin = 5;
	cmds_conf.current = 55;
	cmds_conf.end = 550;
	assert_int_equal(0, execute_cmd("%udf"));
	assert_int_equal(5, user_cmd_info.begin);
	assert_int_equal(550, user_cmd_info.end);
}

TEST(does_not_clash_with_builtins)
{
	move_cmd_called = 0;
	assert_success(execute_cmd("m!"));
	assert_true(move_cmd_called);

	move_cmd_called = 0;
	assert_success(execute_cmd("m?"));
	assert_true(move_cmd_called);
}

TEST(cant_redefine_builtin_with_suffix)
{
	assert_failure(execute_cmd("command move! a"));
	assert_failure(execute_cmd("command move? a"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
