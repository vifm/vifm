#include <stdlib.h>

#include "seatest.h"

#include "../../src/utils/macros.h"
#include "../../src/macros.h"

static void
test_empty_string_ok(void)
{
	custom_macro_t macros[] =
	{
		{ .letter = 'y', .value = "xx", .uses_left = 0, },
	};

	const char *const pattern = "";
	char * expanded = expand_custom_macros(pattern, ARRAY_LEN(macros), macros);
	assert_string_equal("", expanded);
	free(expanded);
}

static void
test_no_macros_ok(void)
{
	custom_macro_t macros[] =
	{
		{ .letter = 'i', .value = "xyz", .uses_left = 0, },
	};

	const char *const pattern = "no match in here";
	char * expanded = expand_custom_macros(pattern, ARRAY_LEN(macros), macros);
	assert_string_equal("no match in here", expanded);
	free(expanded);
}

static void
test_macro_substitution_works(void)
{
	custom_macro_t macros[] =
	{
		{ .letter = 'i', .value = "xyz", .uses_left = 0, },
	};

	const char *const pattern = "a match here %i";
	char * expanded = expand_custom_macros(pattern, ARRAY_LEN(macros), macros);
	assert_string_equal("a match here xyz", expanded);
	free(expanded);
}

static void
test_use_negative_count_macro_not_added_implicitly(void)
{
	custom_macro_t macros[] =
	{
		{ .letter = 'i', .value = "xyz", .uses_left = -1, },
	};

	const char *const pattern = "a match here, %i, just was";
	char * expanded = expand_custom_macros(pattern, ARRAY_LEN(macros), macros);
	assert_string_equal("a match here, xyz, just was", expanded);
	free(expanded);
}

static void
test_use_count_0_macro_not_added_implicitly(void)
{
	custom_macro_t macros[] =
	{
		{ .letter = 'i', .value = "xyz", .uses_left = 0, },
	};

	const char *const pattern = "a match here, %i, just was";
	char * expanded = expand_custom_macros(pattern, ARRAY_LEN(macros), macros);
	assert_string_equal("a match here, xyz, just was", expanded);
	free(expanded);
}

static void
test_use_count_1_macro_added_once_implicitly(void)
{
	custom_macro_t macros[] =
	{
		{ .letter = 'i', .value = "xyz", .uses_left = 1, },
	};

	const char *const pattern = "a match here:";
	char * expanded = expand_custom_macros(pattern, ARRAY_LEN(macros), macros);
	assert_string_equal("a match here: xyz", expanded);
	free(expanded);
}

static void
test_use_count_1_macro_not_added_implicitly(void)
{
	custom_macro_t macros[] =
	{
		{ .letter = 'i', .value = "xyz", .uses_left = 1, },
	};

	const char *const pattern = "a match here, %i, just was";
	char * expanded = expand_custom_macros(pattern, ARRAY_LEN(macros), macros);
	assert_string_equal("a match here, xyz, just was", expanded);
	free(expanded);
}

static void
test_use_count_2_macro_added_once_implicitly(void)
{
	custom_macro_t macros[] =
	{
		{ .letter = 'i', .value = "xyz", .uses_left = 2, },
	};

	const char *const pattern = "a match here, %i, just was,";
	char * expanded = expand_custom_macros(pattern, ARRAY_LEN(macros), macros);
	assert_string_equal("a match here, xyz, just was, xyz", expanded);
	free(expanded);
}

static void
test_use_count_2_macro_added_twice_implicitly(void)
{
	custom_macro_t macros[] =
	{
		{ .letter = 'i', .value = "xyz", .uses_left = 2, },
	};

	const char *const pattern = "matches follow";
	char * expanded = expand_custom_macros(pattern, ARRAY_LEN(macros), macros);
	assert_string_equal("matches follow xyz xyz", expanded);
	free(expanded);
}

static void
test_unknown_macro_removed(void)
{
	custom_macro_t macros[] =
	{
		{ .letter = 'i', .value = "xyz", .uses_left = 0, },
	};

	const char *const pattern = "the macro %b was here";
	char * expanded = expand_custom_macros(pattern, ARRAY_LEN(macros), macros);
	assert_string_equal("the macro  was here", expanded);
	free(expanded);
}

static void
test_double_percent_handled_correctly(void)
{
	custom_macro_t macros[] =
	{
		{ .letter = 'i', .value = "xyz", .uses_left = 0, },
	};

	const char *const pattern = "the percent sign is here: %%";
	char * expanded = expand_custom_macros(pattern, ARRAY_LEN(macros), macros);
	assert_string_equal("the percent sign is here: %", expanded);
	free(expanded);
}

static void
test_ends_with_percent_ok(void)
{
	custom_macro_t macros[] =
	{
		{ .letter = 'i', .value = "xyz", .uses_left = 0, },
	};

	const char *const pattern = "the percent sign is here: %";
	char * expanded = expand_custom_macros(pattern, ARRAY_LEN(macros), macros);
	assert_string_equal("the percent sign is here: %", expanded);
	free(expanded);
}

void
expand_custom_macros_tests(void)
{
	test_fixture_start();

	run_test(test_empty_string_ok);
	run_test(test_no_macros_ok);
	run_test(test_macro_substitution_works);
	run_test(test_use_negative_count_macro_not_added_implicitly);
	run_test(test_use_count_0_macro_not_added_implicitly);
	run_test(test_use_count_1_macro_added_once_implicitly);
	run_test(test_use_count_1_macro_not_added_implicitly);
	run_test(test_use_count_2_macro_added_once_implicitly);
	run_test(test_use_count_2_macro_added_twice_implicitly);
	run_test(test_unknown_macro_removed);
	run_test(test_double_percent_handled_correctly);
	run_test(test_ends_with_percent_ok);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
