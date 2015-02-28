#include <stic.h>

#include <string.h>

#include "../../src/engine/cmds.h"

extern cmds_conf_t cmds_conf;

cmd_info_t user_cmd_info;

SETUP()
{
	assert_int_equal(0, execute_cmd("command udf a"));
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
