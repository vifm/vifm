#include "seatest.h"

#include "../../src/commands.h"

static void
test_whole_line_comments(void)
{
	assert_int_equal(0, execute_command(NULL, "\""));
	assert_int_equal(0, execute_command(NULL, " \""));
	assert_int_equal(0, execute_command(NULL, "  \""));
}

void
comments_tests(void)
{
	test_fixture_start();

	run_test(test_whole_line_comments);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
