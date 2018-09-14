#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h>

#include "../../src/engine/cmds.h"
#include "../../src/utils/string_array.h"

#include "suite.h"

static int move_cmd(const cmd_info_t *cmd_info);
static int dummy_cmd(const cmd_info_t *cmd_info);
static int usercmd_cmd(const cmd_info_t *cmd_info);

extern cmds_conf_t cmds_conf;

cmd_info_t user_cmd_info;

static int move_cmd_called;
static int dummy_cmd_called;

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

TEST(udc_can_add_suffix_to_builtin_cmd)
{
	static const cmd_add_t dummy = {
	  .name = "dummy",       .abbr = NULL,   .id = -1,          .descr = "descr",
	  .flags = 0,
	  .handler = &dummy_cmd, .min_args = 0,  .max_args = 0,
	};

	user_cmd_handler = &usercmd_cmd;

	add_builtin_commands(&dummy, 1);

	assert_success(execute_cmd("command dummy! move"));
	move_cmd_called = 0;
	dummy_cmd_called = 0;
	assert_success(execute_cmd("dummy!"));
	assert_true(move_cmd_called);
	assert_false(dummy_cmd_called);
}

TEST(cant_redefine_builtin_with_suffix)
{
	assert_int_equal(CMDS_ERR_NO_BUILTIN_REDEFINE,
			execute_cmd("command move! a"));
	assert_int_equal(CMDS_ERR_NO_BUILTIN_REDEFINE,
			execute_cmd("command move? a"));
}

TEST(self_deletion_does_not_have_use_after_free)
{
	user_cmd_handler = &usercmd_cmd;

	assert_success(execute_cmd("command suicideone comclear"));
	assert_success(execute_cmd("command suicidetwo suicideone"));
	assert_success(execute_cmd("command suicidethree suicidetwo"));
	assert_success(execute_cmd("suicidethree"));
}

TEST(running_udc_by_ambiguous_prefix_is_not_allowed)
{
	assert_int_equal(0, execute_cmd("command aa a"));
	assert_int_equal(0, execute_cmd("command ab a"));

	assert_int_equal(CMDS_ERR_UDF_IS_AMBIGUOUS, execute_cmd("a"));
}

TEST(removal_of_unknown_udc_fails)
{
	assert_failure(execute_cmd("delcommand nosuchcmd"));
}

TEST(udc_can_be_removed)
{
	move_cmd_called = 0;
	assert_success(execute_cmd("delcommand m"));
	assert_success(execute_cmd("m"));
	assert_false(move_cmd_called);
}

TEST(all_udcs_are_listed)
{
	char **list = list_udf();
	int len = count_strings(list);

	assert_int_equal(6, len);
	assert_string_equal("mkcd!", list[0]);
	assert_string_equal("a", list[1]);
	assert_string_equal("mkcd?", list[2]);
	assert_string_equal("a", list[3]);
	assert_string_equal("udf", list[4]);
	assert_string_equal("a", list[5]);

	free_string_array(list, len);
}

TEST(selected_udcs_are_listed)
{
	assert_string_equal(NULL, list_udf_content("badprefix"));

	const char *expected = "Command -- Action\n"
	                       "mkcd!      a\n"
	                       "mkcd?      a";
	char *list = list_udf_content("mk");
	assert_string_equal(expected, list);
	free(list);
}

TEST(recursion_is_limited)
{
	user_cmd_handler = &usercmd_cmd;

	assert_int_equal(0, execute_cmd("command deeper deeper"));
	assert_int_equal(CMDS_ERR_LOOP, execute_cmd("deeper"));
}

TEST(cant_redefine_command_without_bang)
{
	assert_int_equal(0, execute_cmd("command cmd bla"));
	assert_int_equal(CMDS_ERR_NEED_BANG, execute_cmd("command cmd bla"));
}

TEST(can_redefine_command_with_bang)
{
	assert_int_equal(0, execute_cmd("command cmd bla"));
	assert_int_equal(0, execute_cmd("command! cmd bla"));
}

TEST(can_handle_extra_formats_of_command_builtin)
{
	static const cmd_add_t cmd = {
	  .name = "command",    .abbr = NULL,  .id = -1,            .descr = "descr",
	  .flags = HAS_EMARK | HAS_RANGE | HAS_BG_FLAG | HAS_QUOTED_ARGS
	         | HAS_QMARK_NO_ARGS | HAS_SELECTION_SCOPE,
	  .handler = &dummy_cmd, .min_args = 0, .max_args = NOT_DEF,
	};

	assert_int_equal(CMDS_ERR_TOO_FEW_ARGS, execute_cmd("command cmd"));
	add_builtin_commands(&cmd, 1);
	assert_int_equal(0, execute_cmd("command cmd"));
}

static int
dummy_cmd(const cmd_info_t *cmd_info)
{
	return 0;
}

static int
usercmd_cmd(const cmd_info_t *cmd_info)
{
	return execute_cmd(cmd_info->cmd);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
