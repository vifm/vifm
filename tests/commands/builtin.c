#include <stdlib.h>
#include <string.h>

#include "seatest.h"

#include "../../src/engine/cmds.h"
#include "../../src/engine/completion.h"
#include "../../src/commands.h"

extern struct cmds_conf cmds_conf;

static void
setup(void)
{
	reset_cmds();
	init_commands();
}

static void
teardown(void)
{
}

static void
test_empty_line_completion(void)
{
	char *buf;

	reset_completion();
	assert_int_equal(0, complete_cmd(""));

	buf = next_completion();
	assert_string_equal("!", buf);
	free(buf);

	buf = next_completion();
	assert_string_equal("alink", buf);
	free(buf);
}

static void
test_set(void)
{
	char *buf;

	reset_completion();
	assert_int_equal(0, complete_cmd("se"));

	buf = next_completion();
	assert_string_equal("set", buf);
	free(buf);
}

void
builtin_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_empty_line_completion);
	run_test(test_set);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
