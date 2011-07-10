#include <string.h>

#include "seatest.h"

#include "../../src/commands.h"

static void
setup(void)
{
	add_command("bar", "");
	add_command("baz", "");
	add_command("foo", "");
}

static void
leave_spaces_at_begin(void)
{
	char *buf;

	buf = command_completion(" q", 0);
	assert_string_equal(" quit", buf);
	free(buf);
	buf = command_completion(NULL, 0);
	assert_string_equal(" quit", buf);
	free(buf);
}

static void
only_user(void)
{
	char *buf;

	buf = command_completion("command ", 1);
	assert_string_equal("command bar", buf);
	free(buf);

	buf = command_completion(" command ", 1);
	assert_string_equal(" command bar", buf);
	free(buf);

	buf = command_completion("  command ", 1);
	assert_string_equal("  command bar", buf);
	free(buf);
}

void
test_cmdline_completion(void)
{
	test_fixture_start();

	fixture_setup(setup);

	run_test(leave_spaces_at_begin);
	run_test(only_user);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
