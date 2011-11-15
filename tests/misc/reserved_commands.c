#include <string.h>

#include "seatest.h"

#include "../../src/cmds.h"
#include "../../src/commands.h"
#include "../../src/completion.h"

static void
vim_like_completion(void)
{
	char *buf;

	init_commands();

	reset_completion();
	assert_int_equal(0, complete_cmd("e"));
	buf = next_completion();
	assert_string_equal("edit", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("empty", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("execute", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("exit", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("e", buf);
	free(buf);

	reset_completion();
	assert_int_equal(0, complete_cmd("vm"));
	buf = next_completion();
	assert_string_equal("vmap", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("vmap", buf);
	free(buf);

	reset_completion();
	assert_int_equal(0, complete_cmd("j"));
	buf = next_completion();
	assert_string_equal("jobs", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("jobs", buf);
	free(buf);

	reset_cmds();
}

void
test_reserved_commands(void)
{
	test_fixture_start();

	run_test(vim_like_completion);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
