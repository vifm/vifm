#include "seatest.h"

#include "../../src/commands.h"

static void
test_whole_line_comments(void)
{
	assert_int_equal(0, exec_command("\"", NULL, GET_COMMAND));
	assert_int_equal(0, exec_command(" \"", NULL, GET_COMMAND));
	assert_int_equal(0, exec_command("  \"", NULL, GET_COMMAND));
}

void
comments_tests(void)
{
	test_fixture_start();

	run_test(test_whole_line_comments);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
