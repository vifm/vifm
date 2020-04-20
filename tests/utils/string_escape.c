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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
