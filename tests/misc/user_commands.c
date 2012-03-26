#include <string.h>

#include "seatest.h"

#include "../../src/engine/cmds.h"
#include "../../src/engine/completion.h"
#include "../../src/commands.h"

static void
setup(void)
{
	init_commands();

	execute_cmd("command bar a");
	execute_cmd("command baz b");
	execute_cmd("command foo c");
}

static void
teardown(void)
{
	reset_cmds();
}

static void
comm_completion(void)
{
	char *buf;

	reset_completion();
	assert_int_equal(5, complete_cmd("comm b"));
	buf = next_completion();
	assert_string_equal("bar", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("baz", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("b", buf);
	free(buf);

	reset_completion();
	assert_int_equal(5, complete_cmd("comm f"));
	buf = next_completion();
	assert_string_equal("foo", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("foo", buf);
	free(buf);

	reset_completion();
	assert_int_equal(5, complete_cmd("comm b"));
	buf = next_completion();
	assert_string_equal("bar", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("baz", buf);
	free(buf);
}

void
test_user_commands(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(comm_completion);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
