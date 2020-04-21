#include <stic.h>

#include <locale.h> /* LC_ALL setlocale() */
#include <stdlib.h> /* free() */

#include <test-utils.h>

#include "../../src/utils/str.h"

SETUP_ONCE()
{
	try_enable_utf8_locale();
}

TEST(empty_string_turns_into_empty_string)
{
	char *const narrow = to_multibyte(L"");
	assert_string_equal("", narrow);
	free(narrow);
}

TEST(non_empty_string_turns_into_non_empty_string)
{
	char *const narrow = to_multibyte(L"abcde");
	assert_string_equal("abcde", narrow);
	free(narrow);
}

TEST(non_empty_non_latin_string_turns_into_non_empty_string, IF(utf8_locale))
{
	char *const narrow = to_multibyte(L"абвгд");
	assert_string_equal("абвгд", narrow);
	free(narrow);
}

TEST(conversion_failure_is_detected, IF(not_windows))
{
	/* On Windows narrow locale doesn't really matter because UTF-8 is used
	 * internally by vifm. */
	(void)setlocale(LC_ALL, "C");
	char *const narrow = to_multibyte(L"абвгд");
	assert_string_equal(NULL, narrow);
	free(narrow);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
