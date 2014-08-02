#include <stdlib.h> /* free() */

#include "seatest.h"

#include "../../src/utils/utils.h"

static void
test_squoted_escaping_doubles_single_quote(void)
{
	char *const escaped = escape_for_squotes("0a'b'c'd1", 0);
	assert_string_equal("0a''b''c''d1", escaped);
	free(escaped);
}

static void
test_dquoted_escaping_escapes_special_chars(void)
{
	char *const escaped = escape_for_dquotes("-\aa\bb\fc\nd\re\tf\vg", 0);
	assert_string_equal("-\\aa\\bb\\fc\\nd\\re\\tf\\vg", escaped);
	free(escaped);
}

static void
test_dquoted_escaping_escapes_double_quote(void)
{
	char *const escaped = escape_for_dquotes("0a\"b\"c\"d1", 0);
	assert_string_equal("0a\\\"b\\\"c\\\"d1", escaped);
	free(escaped);
}

void
string_escape_tests(void)
{
	test_fixture_start();

	run_test(test_squoted_escaping_doubles_single_quote);
	run_test(test_dquoted_escaping_escapes_special_chars);
	run_test(test_dquoted_escaping_escapes_double_quote);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
