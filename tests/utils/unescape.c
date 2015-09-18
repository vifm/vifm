#include <stic.h>

#include "../../src/utils/str.h"

TEST(empty_string_untouched)
{
	char escaped[] = "";
	const char *expected = "";
	unescape(escaped, 0);
	assert_string_equal(expected, escaped);
}

TEST(stops_at_the_end)
{
	char escaped[] = "a\\\0b";
	const char *expected = "a";
	unescape(escaped, 0);
	assert_string_equal(expected, escaped);
	assert_int_equal('\0', escaped[2]);
}

TEST(incomplete_escape_sequence_truncated)
{
	char escaped[] = "a\\";
	const char *expected = "a";
	unescape(escaped, 0);
	assert_string_equal(expected, escaped);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
