#include <stic.h>

#include <locale.h> /* setlocale() */
#include <stddef.h> /* size_t */
#include <stdlib.h> /* free() */
#include <string.h> /* strlen() */
#include <wchar.h>

#include "../../src/utils/utf8.h"
#include "../../src/utils/utils.h"

static int locale_works(void);

SETUP_ONCE()
{
	(void)setlocale(LC_ALL, "");
	if(!locale_works())
	{
		(void)setlocale(LC_ALL, "en_US.utf8");
	}
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

TEST(get_real_string_width_in_the_middle_a, IF(locale_works))
{
#define ENDING "丝刀"
	const char utf8_str[] = "师螺" ENDING;
	const char utf8_end[] = ENDING;
#undef ENDING
	const size_t expected_len = strlen(utf8_str) - strlen(utf8_end);
	const size_t calculated_len = utf8_strsnlen(utf8_str, 5);
	assert_int_equal(expected_len, calculated_len);
}

TEST(get_real_string_width_in_the_middle_b, IF(locale_works))
{
#define ENDING "丝刀"
	const char utf8_str[] = "师从螺" ENDING;
	const char utf8_end[] = ENDING;
#undef ENDING
	const size_t expected_len = strlen(utf8_str) - strlen(utf8_end);
	const size_t calculated_len = utf8_strsnlen(utf8_str, 7);
	assert_int_equal(expected_len, calculated_len);
}

TEST(length_is_less_or_equal_to_string_length, IF(locale_works))
{
	const char *str = "01 R\366yksopp - You Know I Have To Go (\326z"
	                  "g\374r \326zkan 5 AM Edit).mp3";
	const size_t len = strlen(str);
	int i;

	for(i = 0; i < len; ++i)
	{
		assert_true(utf8_nstrsnlen(str, i) <= i);
	}
}

static int
locale_works(void)
{
	return (vifm_wcwidth(L'丝') == 2);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
