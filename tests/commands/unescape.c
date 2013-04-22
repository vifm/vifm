#include "seatest.h"

#include "../../src/engine/cmds.h"

static void
test_empty_string_untouched(void)
{
	char escaped[] = "";
	const char *expected = "";
	unescape(escaped, 0);
	assert_string_equal(expected, escaped);
}

static void
test_stops_at_the_end(void)
{
	char escaped[] = "a\\\0b";
	const char *expected = "a";
	unescape(escaped, 0);
	assert_string_equal(expected, escaped);
	assert_int_equal('\0', escaped[2]);
}

static void
test_incomplete_escape_sequence_truncated(void)
{
	char escaped[] = "a\\";
	const char *expected = "a";
	unescape(escaped, 0);
	assert_string_equal(expected, escaped);
}

void
unescape_tests(void)
{
	test_fixture_start();

	run_test(test_empty_string_untouched);
	run_test(test_stops_at_the_end);
	run_test(test_incomplete_escape_sequence_truncated);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
