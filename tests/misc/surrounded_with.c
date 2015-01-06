#include "seatest.h"

#include "../../src/utils/str.h"

static void
test_empty_string_false(void)
{
	assert_false(surrounded_with("", '{', '}'));
	assert_false(surrounded_with("", '\0', '\0'));
}

static void
test_singe_character_string_false(void)
{
	assert_false(surrounded_with("{", '{', '}'));
	assert_false(surrounded_with("}", '{', '}'));

	assert_false(surrounded_with("/", '/', '/'));
}

static void
test_empty_substring_false(void)
{
	assert_false(surrounded_with("{}", '{', '}'));
}

static void
test_nonempty_substring_true(void)
{
	assert_true(surrounded_with("{a}", '{', '}'));
	assert_true(surrounded_with("{ab}", '{', '}'));
	assert_true(surrounded_with("{abc}", '{', '}'));
}

void
surrounded_with_tests(void)
{
	test_fixture_start();

	run_test(test_empty_string_false);
	run_test(test_singe_character_string_false);
	run_test(test_empty_substring_false);
	run_test(test_nonempty_substring_true);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
