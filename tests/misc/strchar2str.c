#include <stic.h>

#include <locale.h> /* setlocale() */
#include <string.h>

#include "../../src/cfg/config.h"
#include "../../src/utils/utils.h"
#include "../../src/escape.h"

static int locale_works(void);

SETUP_ONCE()
{
	(void)setlocale(LC_ALL, "");
	if(!locale_works())
	{
		(void)setlocale(LC_ALL, "en_US.utf8");
	}

	cfg.tab_stop = 8;
}

TEST(chinese_character_width_is_determined_correctly, IF(locale_works))
{
	size_t screen_width;
	const char *const str = strchar2str("师从刀", 0, &screen_width);
	assert_string_equal("师", str);
	assert_int_equal(2, screen_width);
}

TEST(cyrillic_character_width_is_determined_correctly, IF(locale_works))
{
	size_t screen_width;
	const char *const str = strchar2str("йклм", 0, &screen_width);
	assert_string_equal("й", str);
	assert_int_equal(1, screen_width);
}

TEST(tabulation_is_expanded_properly)
{
	int pos;
	for(pos = 0; pos < cfg.tab_stop; pos++)
	{
		size_t screen_width;
		char out_buf[cfg.tab_stop + 1];
		const char *const str = strchar2str("\t", pos, &screen_width);
		const int space_count = cfg.tab_stop - pos%cfg.tab_stop;
		memset(out_buf, ' ', space_count);
		out_buf[space_count] = '\0';

		assert_string_equal(out_buf, str);
		assert_int_equal(space_count, screen_width);
	}
}

TEST(space_is_untouched_and_has_width_of_one_character)
{
	size_t screen_width;
	const char *const str = strchar2str(" abc", 0, &screen_width);
	assert_string_equal(" ", str);
	assert_int_equal(1, screen_width);
}

TEST(newline_is_ignored_and_has_zero_width)
{
	size_t screen_width;
	const char *const str = strchar2str("\nabc", 0, &screen_width);
	assert_string_equal("", str);
	assert_int_equal(0, screen_width);
}

TEST(caret_return_is_converted_to_cr_and_has_proper_width)
{
	size_t screen_width;
	const char *const str = strchar2str("\rabc", 0, &screen_width);
	assert_string_equal("<cr>", str);
	assert_int_equal(4, screen_width);
}

TEST(escape_sequences_are_untouched_and_have_proper_width)
{
	size_t screen_width;
	const char *const str = strchar2str("\033[32m[", 0, &screen_width);
	assert_string_equal("\033[32m", str);
	assert_int_equal(0, screen_width);
}

TEST(ctrl_chars_are_converted_and_have_proper_width)
{
	const char* exceptions = "\b\t\r\n\033";
	char c;
	for(c = '\x01'; c < ' '; c++)
	{
		if(strchr(exceptions, c) == NULL)
		{
			size_t screen_width;
			char in_buf[] = { c, '\0' };
			char out_buf[] = { '^', 'A' + (c - 1), '\0' };
			const char *const str = strchar2str(in_buf, 0, &screen_width);
			assert_string_equal(out_buf, str);
			assert_int_equal(2, screen_width);
		}
	}
}

static int
locale_works(void)
{
	return (vifm_wcwidth(L'丝') == 2);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
