#include "seatest.h"

#include <stdlib.h> /* free() */

#include "../../src/utils/str.h"

static void
test_no_formatting_ok(void)
{
#define STR "No formatting"
	char *str = format_str(STR);
	assert_string_equal(STR, str);
	free(str);
#undef STR
}

static void
test_formatting_ok(void)
{
	char *str = format_str("String %swith%s formatting", "'", "'");
	assert_string_equal("String 'with' formatting", str);
	free(str);
}

void
format_str_tests(void)
{
	test_fixture_start();

	run_test(test_no_formatting_ok);
	run_test(test_formatting_ok);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
