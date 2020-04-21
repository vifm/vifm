#include <stic.h>

#include <stddef.h> /* size_t */
#include <stdlib.h> /* free() */
#include <string.h> /* strlen() */
#include <wchar.h>

#include <test-utils.h>

#include "../../src/utils/str.h"
#include "../../src/utils/utf8.h"

SETUP_ONCE()
{
	try_enable_utf8_locale();
}

TEST(get_real_string_width_full)
{
	const char utf8_str[] = "师вгд";
	const size_t expected_len = strlen(utf8_str);
	const size_t calculated_len = utf8_strsnlen(utf8_str, 5);
	assert_int_equal(expected_len, calculated_len);
}

TEST(tabulation_is_counted_correctly)
{
	const char utf8_str[] = "ab\tcd";
	assert_int_equal(5, utf8_strsw_with_tabs(utf8_str, 1));
	assert_int_equal(6, utf8_strsw_with_tabs(utf8_str, 2));
	assert_int_equal(5, utf8_strsw_with_tabs(utf8_str, 3));
	assert_int_equal(10, utf8_strsw_with_tabs(utf8_str, 8));
}

TEST(get_real_string_width_in_the_middle_a, IF(utf8_locale))
{
#define ENDING "丝刀"
	const char utf8_str[] = "师螺" ENDING;
	const char utf8_end[] = ENDING;
#undef ENDING
	const size_t expected_len = strlen(utf8_str) - strlen(utf8_end);
	const size_t calculated_len = utf8_strsnlen(utf8_str, 5);
	assert_int_equal(expected_len, calculated_len);
}

TEST(get_real_string_width_in_the_middle_b, IF(utf8_locale))
{
#define ENDING "丝刀"
	const char utf8_str[] = "师从螺" ENDING;
	const char utf8_end[] = ENDING;
#undef ENDING
	const size_t expected_len = strlen(utf8_str) - strlen(utf8_end);
	const size_t calculated_len = utf8_strsnlen(utf8_str, 7);
	assert_int_equal(expected_len, calculated_len);
}

TEST(length_is_less_or_equal_to_string_length, IF(utf8_locale))
{
	const char *str = "01 R\366yksopp - You Know I Have To Go (\326z"
	                  "g\374r \326zkan 5 AM Edit).mp3";
	const size_t len = strlen(str);
	size_t i;

	for(i = 0; i < len; ++i)
	{
		assert_true(utf8_nstrsnlen(str, i) <= i);
	}
}

#ifdef _WIN32

TEST(utf16_roundtrip, IF(utf8_locale))
{
	const wchar_t str[] = { 0x79d8, 0 };

	char *const utf8 = utf8_from_utf16(str);
	wchar_t *const utf16 = utf8_to_utf16(utf8);
	assert_true(wcscmp(str, utf16) == 0);
	free(utf16);
	free(utf8);
}

TEST(first_char, IF(utf8_locale))
{
	const wchar_t str[] = { 0x79d8, 0 };
	char *const utf8 = utf8_from_utf16(str);
	assert_int_equal(0x79d8, get_first_wchar(utf8));
	free(utf8);
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
