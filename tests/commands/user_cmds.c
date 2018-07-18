#include <stic.h>

#include <string.h>

#include "../../src/engine/cmds.h"

#include "suite.h"

static int move_cmd(const cmd_info_t *cmd_info);
static int usercmd_cmd(const cmd_info_t *cmd_info);

extern cmds_conf_t cmds_conf;

cmd_info_t user_cmd_info;

static int move_cmd_called;

SETUP()
{
	static const cmd_add_t move = {
	  .name = "move",       .abbr = "m",   .id = -1,            .descr = "descr",
	  .flags = HAS_EMARK | HAS_RANGE | HAS_BG_FLAG | HAS_QUOTED_ARGS
	         | HAS_QMARK_NO_ARGS | HAS_SELECTION_SCOPE,
	  .handler = &move_cmd, .min_args = 0, .max_args = NOT_DEF,
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

TEST(self_deletion_does_not_have_use_after_free)
{
	user_cmd_handler = &usercmd_cmd;

	assert_success(execute_cmd("command suicideone comclear"));
	assert_success(execute_cmd("command suicidetwo suicideone"));
	assert_success(execute_cmd("command suicidethree suicidetwo"));
	assert_success(execute_cmd("suicidethree"));
}

static int
usercmd_cmd(const cmd_info_t *cmd_info)
{
	return execute_cmd(cmd_info->cmd);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
