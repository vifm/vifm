#include <string.h>

#include "seatest.h"

#include "../../src/config.h"
#include "../../src/commands.h"

static void
setup(void)
{
	cfg.command_num = 0;

	add_command("comix", "do");
}

static void
teardown(void)
{
	remove_command("comix");
}

static void
command_that_begins_with_com(void)
{
	cmd_params cmd;

	assert_true(parse_command(NULL, "com! comix donot", &cmd) > 0);
}

void
user_command_and_exclamation_mark(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(command_that_begins_with_com);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
