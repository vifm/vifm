#include <string.h>

#include "seatest.h"

#include "../../src/commands.h"

static void
should_be_sorted(void)
{
	int i;

	for(i = 0; i < RESERVED - 1; i++)
	{
		assert_true(strcmp(reserved_cmds[i].name,
				reserved_cmds[i + 1].name) < 0);
	}
}

static void
vim_like_completion(void)
{
	char *buf;

	buf = command_completion("e", 0);
	assert_string_equal("edit", buf);
	free(buf);
	buf = command_completion(NULL, 0);
	assert_string_equal("empty", buf);
	free(buf);
	buf = command_completion(NULL, 0);
	assert_string_equal("e", buf);
	free(buf);

	buf = command_completion("vm", 0);
	assert_string_equal("vmap", buf);
	free(buf);
	buf = command_completion(NULL, 0);
	assert_string_equal("vmap", buf);
	free(buf);

	buf = command_completion("j", 0);
	assert_string_equal("jobs", buf);
	free(buf);
	buf = command_completion(NULL, 0);
	assert_string_equal("jobs", buf);
	free(buf);
}

void
test_reserved_commands(void)
{
	test_fixture_start();

	run_test(should_be_sorted);
	run_test(vim_like_completion);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
