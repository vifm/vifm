#include <string.h>

#include "seatest.h"

#include "../../src/engine/cmds.h"

extern cmds_conf_t cmds_conf;

cmd_info_t user_cmd_info;

static void
setup(void)
{
}

static void
teardown(void)
{
}

static void
test_empty_ok(void)
{
	const char cmd[] = "";
	size_t len;
	const char *last;
	last = get_last_argument(cmd, &len);

	assert_true(last == cmd);
	assert_int_equal(0, len);
}

static void
test_one_word_ok(void)
{
	const char cmd[] = "a";
	size_t len;
	const char *last;
	last = get_last_argument(cmd, &len);

	assert_true(last == cmd);
	assert_int_equal(1, len);
}

static void
test_two_words_ok(void)
{
	const char cmd[] = "b a";
	size_t len;
	const char *last;
	last = get_last_argument(cmd, &len);

	assert_true(last == cmd + 2);
	assert_int_equal(1, len);
}

static void
test_trailing_spaces_ok(void)
{
	const char cmd[] = "a    ";
	size_t len;
	const char *last;
	last = get_last_argument(cmd, &len);

	assert_true(last == cmd);
	assert_int_equal(1, len);
}

static void
test_single_quotes_ok(void)
{
	const char cmd[] = "b  'hello'";
	size_t len;
	const char *last;
	last = get_last_argument(cmd, &len);

	assert_true(last == cmd + 3);
	assert_int_equal(7, len);
}

static void
test_double_quotes_ok(void)
{
	const char cmd[] = "b \"hi\"";
	size_t len;
	const char *last;
	last = get_last_argument(cmd, &len);

	assert_true(last == cmd + 2);
	assert_int_equal(4, len);
}

static void
test_ending_space_ok(void)
{
	const char cmd[] = "b a\\ ";
	size_t len;
	const char *last;
	last = get_last_argument(cmd, &len);

	assert_true(last == cmd + 2);
	assert_int_equal(3, len);
}

void
last_argument_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_empty_ok);
	run_test(test_one_word_ok);
	run_test(test_two_words_ok);
	run_test(test_trailing_spaces_ok);
	run_test(test_single_quotes_ok);
	run_test(test_double_quotes_ok);
	run_test(test_ending_space_ok);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
