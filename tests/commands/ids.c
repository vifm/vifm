#include <string.h>

#include "seatest.h"

#include "../../src/engine/cmds.h"

extern cmds_conf_t cmds_conf;

cmd_info_t user_cmd_info;

static void
setup(void)
{
}

static void
teardown(void)
{
}

static void
test_builtin(void)
{
	assert_int_equal(COMMAND_CMD_ID, get_cmd_id("com"));
	assert_int_equal(COMMAND_CMD_ID, get_cmd_id("command"));

	assert_int_equal(DELCOMMAND_CMD_ID, get_cmd_id("delc"));
	assert_int_equal(DELCOMMAND_CMD_ID, get_cmd_id("delcommand"));
}

static void
test_user(void)
{
	assert_int_equal(0, execute_cmd("command udf a"));

	assert_int_equal(USER_CMD_ID, get_cmd_id("udf"));
}

static void
test_invalid(void)
{
	assert_int_equal(-1, get_cmd_id("asdf"));
}

static void
test_end_characters(void)
{
	assert_int_equal(COMMAND_CMD_ID, get_cmd_id("com#"));
}

void
ids_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_builtin);
	run_test(test_user);
	run_test(test_invalid);
	run_test(test_end_characters);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
