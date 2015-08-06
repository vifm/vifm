#include <stic.h>

#include <string.h>

#include "../../src/engine/cmds.h"

extern cmds_conf_t cmds_conf;

cmd_info_t user_cmd_info;

TEST(builtin)
{
	assert_int_equal(COMMAND_CMD_ID, get_cmd_id("com"));
	assert_int_equal(COMMAND_CMD_ID, get_cmd_id("command"));

	assert_int_equal(DELCOMMAND_CMD_ID, get_cmd_id("delc"));
	assert_int_equal(DELCOMMAND_CMD_ID, get_cmd_id("delcommand"));
}

TEST(user)
{
	assert_int_equal(0, execute_cmd("command udf a"));

	assert_int_equal(USER_CMD_ID, get_cmd_id("udf"));
}

TEST(invalid)
{
	assert_int_equal(-1, get_cmd_id("asdf"));
}

TEST(end_characters)
{
	assert_int_equal(COMMAND_CMD_ID, get_cmd_id("com#"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
