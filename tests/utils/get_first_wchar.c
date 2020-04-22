#include <stic.h>

#include <test-utils.h>

#include "../../src/utils/str.h"

SETUP_ONCE()
{
	try_enable_utf8_locale();
}

TEST(valid_wchar_is_returned, IF(utf8_locale))
{
	assert_int_equal(L'a', get_first_wchar("ab"));
}

#ifndef _WIN32

TEST(broken_char_is_chopped, IF(utf8_locale))
{
	assert_int_equal(L'\361', get_first_wchar("\361b"));
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
