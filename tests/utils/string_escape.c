#include <stic.h>

#include <stdlib.h> /* free() */

#include "../../src/utils/utils.h"

TEST(squoted_escaping_doubles_single_quote)
{
	char *const escaped = escape_for_squotes("0a'b'c'd1", 0);
	assert_string_equal("0a''b''c''d1", escaped);
	free(escaped);
}

TEST(dquoted_escaping_escapes_special_chars)
{
	char *const escaped = escape_for_dquotes("-\aa\bb\fc\nd\re\tf\vg", 0);
	assert_string_equal("-\\aa\\bb\\fc\\nd\\re\\tf\\vg", escaped);
	free(escaped);
}

TEST(dquoted_escaping_escapes_double_quote)
{
	char *const escaped = escape_for_dquotes("0a\"b\"c\"d1", 0);
	assert_string_equal("0a\\\"b\\\"c\\\"d1", escaped);
	free(escaped);
}

TEST(escape_unreadable_escapes_control_codes)
{
	char *const escaped = escape_unreadable("prefix\x01\x02\x03suffix");
	assert_string_equal("prefix^A^B^Csuffix", escaped);
	free(escaped);
}

TEST(escape_unreadable_escapes_unprintable_unicode)
{
	char *const escaped = escape_unreadable("unicode\xe2\x80\x8etest");
	assert_string_equal("unicode^?test", escaped);
	free(escaped);
}

TEST(escape_unreadable_escapes_wrong_encoding)
{
	char *const escaped = escape_unreadable("L\224sungswege, Testf\204lle.pdf");
	assert_string_equal("L^?sungswege, Testf^?lle.pdf", escaped);
	free(escaped);
}

TEST(escape_unreadable_handles_composite_characters)
{
	const char *unescaped = "prefix\x6f\xcc\x88\x61\xcc\x88\x75\xcc\x88\xc3\x9f"
		"\x4f\xcc\x88\x41\xccsuffix";

	char *const escaped = escape_unreadable(unescaped);
	assert_string_equal(unescaped, escaped);
	free(escaped);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
