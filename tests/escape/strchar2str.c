#include <stic.h>

#include <string.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/ui/escape.h"
#include "../../src/utils/utils.h"

SETUP_ONCE()
{
	try_enable_utf8_locale();

	cfg.tab_stop = 8;
}

TEST(chinese_character_width_is_determined_correctly, IF(utf8_locale))
{
	size_t screen_width;
	const char *const str = strchar2str("师从刀", 0, &screen_width);
	assert_string_equal("师", str);
	assert_int_equal(2, screen_width);
}

TEST(cyrillic_character_width_is_determined_correctly, IF(utf8_locale))
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

TEST(broken_escape_sequences_are_eaten)
{
	size_t screen_width;
	const char *const str = strchar2str("\033[", 0, &screen_width);
	assert_string_equal("", str);
	assert_int_equal(0, screen_width);
}

TEST(backspace_is_eaten)
{
	size_t screen_width;
	const char *const str = strchar2str("\b", 0, &screen_width);
	assert_string_equal("", str);
	assert_int_equal(0, screen_width);
}

TEST(long_sequence_is_not_ignored_semicolons)
{
	size_t screen_width;
	const char *str = strchar2str("\033[22;24;25;27;28;21;33;38;5;247;48;5;235m",
			0, &screen_width);
	assert_string_equal("\033[22;24;25;27;28;21;33;38;5;247;48;5;235m", str);
	assert_int_equal(0, screen_width);
}

TEST(long_sequence_is_not_ignored_commas)
{
	size_t screen_width;
	const char *str = strchar2str("\033[22,24,25,27,28,21,33,38,5,247,48,5,235m",
			0, &screen_width);
	assert_string_equal("\033[22,24,25,27,28,21,33,38,5,247,48,5,235m", str);
	assert_int_equal(0, screen_width);
}

TEST(line_erasure_sequence_is_eaten_semicolons)
{
	size_t screen_width;
	const char *str = strchar2str("\033[K\033[22;24;25;27;28;38;5;247;48;5;235m",
			0, &screen_width);
	assert_string_equal("", str);
	assert_int_equal(0, screen_width);
}

TEST(line_erasure_sequence_is_eaten_commas)
{
	size_t screen_width;
	const char *str = strchar2str("\033[K\033[22,24,25,27,28,38,5,247,48,5,235m",
			0, &screen_width);
	assert_string_equal("", str);
	assert_int_equal(0, screen_width);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
