#include "seatest.h"

#include "../../src/utils/str.h"

static void
test_empty_line_returns_null(void)
{
	char part_buf[64];
	assert_string_equal(NULL, extract_part("", ':', part_buf));
}

static void
test_no_delimiters_extracted_correctly(void)
{
	char part_buf[64];
	assert_false(extract_part("abc", ':', part_buf) == NULL);
	assert_string_equal("abc", part_buf);
}

static void
test_single_char_extracted_correctly(void)
{
	char part_buf[64];
	assert_false(extract_part("a:", ':', part_buf) == NULL);
	assert_string_equal("a", part_buf);
}

static void
test_empty_parts_are_skipped(void)
{
	char part_buf[64];
	assert_false(extract_part("::::abc", ':', part_buf) == NULL);
	assert_string_equal("abc", part_buf);
}

void
extract_part_tests(void)
{
	test_fixture_start();

	run_test(test_empty_line_returns_null);
	run_test(test_no_delimiters_extracted_correctly);
	run_test(test_single_char_extracted_correctly);
	run_test(test_empty_parts_are_skipped);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
