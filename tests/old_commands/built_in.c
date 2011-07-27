#include <string.h>

#include "seatest.h"

#include "../../src/completion.h"
#include "../../src/config.h"
#include "../../src/commands.h"

static void
test_set(void)
{
	char *buf;

	reset_completion();
	buf = command_completion("se", 0);
	assert_string_equal("set", buf);
	free(buf);
}

static void
test_empty_line_completion(void)
{
	char *buf;

	reset_completion();
	buf = command_completion("", 0);
	assert_string_equal("!", buf);
	free(buf);

	buf = command_completion(NULL, 0);
	assert_string_equal("apropos", buf);
	free(buf);
}

void
built_in(void)
{
	test_fixture_start();

	run_test(test_set);
	run_test(test_empty_line_completion);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
