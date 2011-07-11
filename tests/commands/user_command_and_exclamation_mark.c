#include <string.h>

#include "seatest.h"

#include "../../src/config.h"
#include "../../src/commands.h"

static void
setup(void)
{
	cfg.command_num = 0;

	add_command("comix", "do");
	add_command("mycmd", "op1");
	add_command("mycmd!", "op2");
}

static void
teardown(void)
{
	remove_command("mycmd");
	remove_command("mycmd!");
	remove_command("comix");
}

static void
command_that_begins_with_com(void)
{
	cmd_params cmd;

	assert_true(parse_command(NULL, "com! comix donot", &cmd) > 0);
}

static void
excl_mark_let_us_differ_them(void)
{
	cmd_params cmd;
	int id1;
	int id2;

	assert_true(parse_command(NULL, "mycmd", &cmd) > 0);
	id1 = cmd.is_user;

	assert_true(parse_command(NULL, "mycmd!", &cmd) > 0);
	id2 = cmd.is_user;

	assert_false(id1 == id2);
}

void
user_command_and_exclamation_mark(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(command_that_begins_with_com);
	run_test(excl_mark_let_us_differ_them);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
