#include <stic.h>

#include "../../src/utils/str.h"

TEST(empty_line_returns_null)
{
	char part_buf[64];
	assert_string_equal(NULL, extract_part("", ":", part_buf));
}

TEST(no_delimiters_extracted_correctly)
{
	char part_buf[64];
	assert_false(extract_part("abc", ":", part_buf) == NULL);
	assert_string_equal("abc", part_buf);
}

TEST(single_char_extracted_correctly)
{
	char part_buf[64];
	assert_false(extract_part("a:", ":", part_buf) == NULL);
	assert_string_equal("a", part_buf);
}

TEST(empty_parts_are_skipped)
{
	char part_buf[64];
	assert_false(extract_part("::::abc", ":", part_buf) == NULL);
	assert_string_equal("abc", part_buf);
}

TEST(multiple_separators)
{
	char part_buf[64];
	assert_false(extract_part(";:abc;:", ":;", part_buf) == NULL);
	assert_string_equal("abc", part_buf);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
