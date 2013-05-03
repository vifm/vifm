/* UTF-8 isn't used on Windows yet. */

#include <locale.h> /* setlocale() */
#include <string.h>

#include "seatest.h"

#include "../../src/cfg/config.h"
#include "../../src/escape.h"

#ifndef _WIN32
/* UTF-8 isn't used on Windows yet. */

static void
test_chinese_character_width_is_determined_correctly(void)
{
	size_t screen_width;
	const char *const str = strchar2str("师从刀", 0, &screen_width);
	assert_string_equal("师", str);
	assert_int_equal(2, screen_width);
}

static void
test_cyrillic_character_width_is_determined_correctly(void)
{
	size_t screen_width;
	const char *const str = strchar2str("йклм", 0, &screen_width);
	assert_string_equal("й", str);
	assert_int_equal(1, screen_width);
}

#endif

static void
test_tabulation_is_expanded_properly(void)
{
	int pos;
	for(pos = 0; pos < cfg.tab_stop; pos++)
	{
		size_t screen_width;
		char out_buf[cfg.tab_stop];
		const char *const str = strchar2str("\t", pos, &screen_width);
		const int space_count = cfg.tab_stop - pos%cfg.tab_stop;
		memset(out_buf, ' ', space_count);
		out_buf[space_count] = '\0';

		assert_string_equal(out_buf, str);
		assert_int_equal(space_count, screen_width);
	}
}

static void
test_space_is_untouched_and_has_width_of_one_character(void)
{
	size_t screen_width;
	const char *const str = strchar2str(" abc", 0, &screen_width);
	assert_string_equal(" ", str);
	assert_int_equal(1, screen_width);
}

static void
test_newline_is_ignored_and_has_zero_width(void)
{
	size_t screen_width;
	const char *const str = strchar2str("\nabc", 0, &screen_width);
	assert_string_equal("", str);
	assert_int_equal(0, screen_width);
}

static void
test_caret_return_is_converted_to_cr_and_has_proper_width(void)
{
	size_t screen_width;
	const char *const str = strchar2str("\rabc", 0, &screen_width);
	assert_string_equal("<cr>", str);
	assert_int_equal(4, screen_width);
}

static void
test_escape_sequences_are_untouched_and_have_proper_width(void)
{
	size_t screen_width;
	const char *const str = strchar2str("\033[32m[", 0, &screen_width);
	assert_string_equal("\033[32m", str);
	assert_int_equal(0, screen_width);
}

static void
test_ctrl_chars_are_converted_and_have_proper_width(void)
{
	const char* exceptions = "\t\r\n\033";
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

void
strchar2str_tests(void)
{
	test_fixture_start();

	(void)setlocale(LC_ALL, "");
	if(wcwidth(L'丝') != 2)
	{
		(void)setlocale(LC_ALL, "en_US.utf8");
	}

	cfg.tab_stop = 8;

#ifndef _WIN32
	/* UTF-8 isn't used on Windows yet. */
	if(wcwidth(L'丝') == 2)
	{
		run_test(test_chinese_character_width_is_determined_correctly);
		run_test(test_cyrillic_character_width_is_determined_correctly);
	}
#endif
	run_test(test_tabulation_is_expanded_properly);
	run_test(test_space_is_untouched_and_has_width_of_one_character);
	run_test(test_newline_is_ignored_and_has_zero_width);
	run_test(test_caret_return_is_converted_to_cr_and_has_proper_width);
	run_test(test_escape_sequences_are_untouched_and_have_proper_width);
	run_test(test_ctrl_chars_are_converted_and_have_proper_width);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
