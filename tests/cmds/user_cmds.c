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

	vle_cmds_add(&move, 1);

	assert_success(vle_cmds_run("command udf a"));
	assert_success(vle_cmds_run("command mkcd! a"));
	assert_success(vle_cmds_run("command mkcd? a"));
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
	assert_int_equal(0, vle_cmds_run("%udf"));
	assert_int_equal(5, user_cmd_info.begin);
	assert_int_equal(550, user_cmd_info.end);
}

TEST(does_not_clash_with_builtins)
{
	move_cmd_called = 0;
	assert_success(vle_cmds_run("m!"));
	assert_true(move_cmd_called);

	move_cmd_called = 0;
	assert_success(vle_cmds_run("m?"));
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

	vle_cmds_add(&dummy, 1);

	assert_success(vle_cmds_run("command dummy! move"));
	move_cmd_called = 0;
	dummy_cmd_called = 0;
	assert_success(vle_cmds_run("dummy!"));
	assert_true(move_cmd_called);
	assert_false(dummy_cmd_called);
}

TEST(cant_redefine_builtin_with_suffix)
{
	assert_int_equal(CMDS_ERR_NO_BUILTIN_REDEFINE,
			vle_cmds_run("command move! a"));
	assert_int_equal(CMDS_ERR_NO_BUILTIN_REDEFINE,
			vle_cmds_run("command move? a"));
}

TEST(self_deletion_does_not_have_use_after_free)
{
	user_cmd_handler = &usercmd_cmd;

	assert_success(vle_cmds_run("command suicideone comclear"));
	assert_success(vle_cmds_run("command suicidetwo suicideone"));
	assert_success(vle_cmds_run("command suicidethree suicidetwo"));
	assert_success(vle_cmds_run("suicidethree"));
}

TEST(running_udc_by_ambiguous_prefix_is_not_allowed)
{
	assert_int_equal(0, vle_cmds_run("command aa a"));
	assert_int_equal(0, vle_cmds_run("command ab a"));

	assert_int_equal(CMDS_ERR_UDF_IS_AMBIGUOUS, vle_cmds_run("a"));
}

TEST(removal_of_unknown_udc_fails)
{
	assert_failure(vle_cmds_run("delcommand nosuchcmd"));
}

TEST(udc_can_be_removed)
{
	move_cmd_called = 0;
	assert_success(vle_cmds_run("delcommand m"));
	assert_success(vle_cmds_run("m"));
	assert_false(move_cmd_called);
}

TEST(all_udcs_are_listed)
{
	char **list = vle_cmds_list_udcs();
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
	assert_string_equal(NULL, vle_cmds_print_udcs("badprefix"));

	const char *expected = "Command -- Action\n"
	                       "mkcd!      a\n"
	                       "mkcd?      a";
	char *list = vle_cmds_print_udcs("mk");
	assert_string_equal(expected, list);
	free(list);
}

TEST(recursion_is_limited)
{
	user_cmd_handler = &usercmd_cmd;

	assert_int_equal(0, vle_cmds_run("command deeper deeper"));
	assert_int_equal(CMDS_ERR_LOOP, vle_cmds_run("deeper"));
}

TEST(cant_redefine_command_without_bang)
{
	assert_int_equal(0, vle_cmds_run("command cmd bla"));
	assert_int_equal(CMDS_ERR_NEED_BANG, vle_cmds_run("command cmd bla"));
}

TEST(can_redefine_command_with_bang)
{
	assert_int_equal(0, vle_cmds_run("command cmd bla"));
	assert_int_equal(0, vle_cmds_run("command! cmd bla"));
}

TEST(can_handle_extra_formats_of_command_builtin)
{
	static const cmd_add_t cmd = {
	  .name = "command",    .abbr = NULL,  .id = -1,            .descr = "descr",
	  .flags = HAS_EMARK | HAS_RANGE | HAS_BG_FLAG | HAS_QUOTED_ARGS
	         | HAS_QMARK_NO_ARGS | HAS_SELECTION_SCOPE,
	  .handler = &dummy_cmd, .min_args = 0, .max_args = NOT_DEF,
	};

	assert_int_equal(CMDS_ERR_TOO_FEW_ARGS, vle_cmds_run("command cmd"));
	vle_cmds_add(&cmd, 1);
	assert_int_equal(0, vle_cmds_run("command cmd"));
}

TEST(cmds_clear_removes_user_commands)
{
	assert_success(vle_cmds_run("command cmd value"));
	vle_cmds_clear();
	assert_success(vle_cmds_run("command cmd value"));
}

static int
dummy_cmd(const cmd_info_t *cmd_info)
{
	return 0;
}

static int
usercmd_cmd(const cmd_info_t *cmd_info)
{
	return vle_cmds_run(cmd_info->user_action);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
