/* UTF-8 isn't used on Windows yet. */
#ifndef _WIN32

/* To get wcswidth() function. */
#define _XOPEN_SOURCE

#include "seatest.h"

#include <locale.h> /* setlocale() */
#include <stdlib.h> /* free() */
#include <string.h> /* strlen() */
#include <wchar.h> /* wcswidth() */

#include "../../src/utils/utf8.h"

/* For wcswidth() stub. */
#ifdef _WIN32
#include "../../src/utils/utils.h"
#endif

static void
test_get_real_string_width_full(void)
{
	const char utf8_str[] = "师вгд";
	const size_t expected_len = strlen(utf8_str);
	const size_t calculated_len = get_real_string_width(utf8_str, 5);
	assert_int_equal(expected_len, calculated_len);
}

static void
test_get_real_string_width_in_the_middle_a(void)
{
#define ENDING "丝刀"
	const char utf8_str[] = "师螺" ENDING;
	const char utf8_end[] = ENDING;
#undef ENDING
	const size_t expected_len = strlen(utf8_str) - strlen(utf8_end);
	const size_t calculated_len = get_real_string_width(utf8_str, 5);
	assert_int_equal(expected_len, calculated_len);
}

static void
test_get_real_string_width_in_the_middle_b(void)
{
#define ENDING "丝刀"
	const char utf8_str[] = "师从螺" ENDING;
	const char utf8_end[] = ENDING;
#undef ENDING
	const size_t expected_len = strlen(utf8_str) - strlen(utf8_end);
	const size_t calculated_len = get_real_string_width(utf8_str, 7);
	assert_int_equal(expected_len, calculated_len);
}

void
utf8_tests(void)
{
	test_fixture_start();

	(void)setlocale(LC_ALL, "");
	if(wcwidth(L'丝') != 2)
	{
		(void)setlocale(LC_ALL, "en_US.utf8");
	}

	run_test(test_get_real_string_width_full);
	if(wcwidth(L'丝') == 2)
	{
		run_test(test_get_real_string_width_in_the_middle_a);
		run_test(test_get_real_string_width_in_the_middle_b);
	}

	test_fixture_end();
}

#else

void
utf8_tests(void)
{
	/* UTF-8 isn't used on Windows yet. */
}

#endif
/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
