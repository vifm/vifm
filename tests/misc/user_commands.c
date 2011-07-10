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
comm_completion(void)
{
	char *buf;

	buf = command_completion("b", 1);
	assert_string_equal("bar", buf);
	free(buf);
	buf = command_completion(NULL, 1);
	assert_string_equal("baz", buf);
	free(buf);
	buf = command_completion(NULL, 1);
	assert_string_equal("b", buf);
	free(buf);

	buf = command_completion("f", 1);
	assert_string_equal("foo", buf);
	free(buf);
	buf = command_completion(NULL, 1);
	assert_string_equal("foo", buf);
	free(buf);

	buf = command_completion("comm b", 1);
	assert_string_equal("comm bar", buf);
	free(buf);
	buf = command_completion(NULL, 1);
	assert_string_equal("comm baz", buf);
	free(buf);
}

void
test_user_commands(void)
{
	test_fixture_start();

	fixture_setup(setup);

	run_test(comm_completion);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
