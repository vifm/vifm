#include <stic.h>

#include <stdlib.h>

#include <test-utils.h>

#include "../../src/ui/colored_line.h"
#include "../../src/utils/macros.h"
#include "../../src/macros.h"

static void check_hi(const char pattern[], size_t nmacros,
		custom_macro_t macros[], int with_opt, const char line[],
		const char attrs[]);

TEST(empty_string_ok)
{
	custom_macro_t macros[] = {
		{ .letter = 'y', .value = "xx", .uses_left = 0, .group = -1, },
	};

	const char *const pattern = "";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros,
			MA_NOOPT);
	assert_string_equal("", expanded);
	free(expanded);
}

TEST(no_macros_ok)
{
	custom_macro_t macros[] = {
		{ .letter = 'i', .value = "xyz", .uses_left = 0, .group = -1, },
	};

	const char *const pattern = "no match in here";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros,
			MA_NOOPT);
	assert_string_equal("no match in here", expanded);
	free(expanded);
}

TEST(macro_substitution_works)
{
	custom_macro_t macros[] = {
		{ .letter = 'i', .value = "xyz", .uses_left = 0, .group = -1, },
	};

	const char *const pattern = "a match here %i";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros,
			MA_NOOPT);
	assert_string_equal("a match here xyz", expanded);
	free(expanded);
}

TEST(use_negative_count_macro_not_added_implicitly)
{
	custom_macro_t macros[] = {
		{ .letter = 'i', .value = "xyz", .uses_left = -1, .group = -1, },
	};

	const char *const pattern = "a match here, %i, just was";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros,
			MA_NOOPT);
	assert_string_equal("a match here, xyz, just was", expanded);
	free(expanded);
}

TEST(use_count_0_macro_not_added_implicitly)
{
	custom_macro_t macros[] = {
		{ .letter = 'i', .value = "xyz", .uses_left = 0, .group = -1, },
	};

	const char *const pattern = "a match here, %i, just was";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros,
			MA_NOOPT);
	assert_string_equal("a match here, xyz, just was", expanded);
	free(expanded);
}

TEST(use_count_1_macro_added_once_implicitly)
{
	custom_macro_t macros[] = {
		{ .letter = 'i', .value = "xyz", .uses_left = 1, .group = -1, },
	};

	const char *const pattern = "a match here:";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros,
			MA_NOOPT);
	assert_string_equal("a match here: xyz", expanded);
	free(expanded);
}

TEST(use_count_1_macro_not_added_implicitly)
{
	custom_macro_t macros[] = {
		{ .letter = 'i', .value = "xyz", .uses_left = 1, .group = -1, },
	};

	const char *const pattern = "a match here, %i, just was";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros,
			MA_NOOPT);
	assert_string_equal("a match here, xyz, just was", expanded);
	free(expanded);
}

TEST(use_count_2_macro_added_once_implicitly)
{
	custom_macro_t macros[] = {
		{ .letter = 'i', .value = "xyz", .uses_left = 2, .group = -1, },
	};

	const char *const pattern = "a match here, %i, just was,";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros,
			MA_NOOPT);
	assert_string_equal("a match here, xyz, just was, xyz", expanded);
	free(expanded);
}

TEST(use_count_2_macro_added_twice_implicitly)
{
	custom_macro_t macros[] = {
		{ .letter = 'i', .value = "xyz", .uses_left = 2, .group = -1, },
	};

	const char *const pattern = "matches follow";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros,
			MA_NOOPT);
	assert_string_equal("matches follow xyz xyz", expanded);
	free(expanded);
}

TEST(unknown_macro_removed)
{
	custom_macro_t macros[] = {
		{ .letter = 'i', .value = "xyz", .uses_left = 0, .group = -1, },
	};

	const char *const pattern = "the macro %b was here";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros,
			MA_NOOPT);
	assert_string_equal("the macro  was here", expanded);
	free(expanded);
}

TEST(double_percent_handled_correctly)
{
	custom_macro_t macros[] = {
		{ .letter = 'i', .value = "xyz", .uses_left = 0, .group = -1, },
	};

	const char *const pattern = "the percent sign is here: %%";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros,
			MA_NOOPT);
	assert_string_equal("the percent sign is here: %", expanded);
	free(expanded);
}

TEST(ends_with_percent_ok)
{
	custom_macro_t macros[] = {
		{ .letter = 'i', .value = "xyz", .uses_left = 0, .group = -1, },
	};

	const char *const pattern = "the percent sign is here: %";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros,
			MA_NOOPT);
	assert_string_equal("the percent sign is here: %", expanded);
	free(expanded);
}

TEST(first_group_member_are_not_added_when_should_not)
{
	custom_macro_t macros[] = {
		{ .letter = 'i', .value = "xyz", .uses_left = 1, .group = 0, },
		{ .letter = 'j', .value = "abc", .uses_left = 0, .group = 0, },
	};

	const char *const pattern = "no i expansion is expected: %j";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros,
			MA_NOOPT);
	assert_string_equal("no i expansion is expected: abc", expanded);
	free(expanded);
}

TEST(second_group_member_are_not_added_when_should_not)
{
	custom_macro_t macros[] = {
		{ .letter = 'i', .value = "xyz", .uses_left = 1, .group = 0, },
		{ .letter = 'j', .value = "abc", .uses_left = 0, .group = 0, },
	};

	const char *const pattern = "no j expansion is expected: %i";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros,
			MA_NOOPT);
	assert_string_equal("no j expansion is expected: xyz", expanded);
	free(expanded);
}

TEST(first_item_of_group_is_repeated_when_needed)
{
	custom_macro_t macros[] = {
		{ .letter = 'i', .value = "xyz", .uses_left = 1, .group = 0, },
		{ .letter = 'j', .value = "abc", .uses_left = 0, .group = 0, },
	};

	const char *const pattern = "i expansion is expected:";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros,
			MA_NOOPT);
	assert_string_equal("i expansion is expected: xyz", expanded);
	free(expanded);
}

TEST(explicit_use_sets_appropriate_flag)
{
	custom_macro_t macros[] = {
		{ .letter = 'u', .value = "", .uses_left = 1, .group = -1 },
		{ .letter = 'U', .value = "", .uses_left = 1, .group = -1 },
	};

	const char *const pattern = "something%u";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros,
			MA_NOOPT);
	assert_string_equal("something", expanded);
	free(expanded);

	assert_true(macros[0].explicit_use);
	assert_false(macros[1].explicit_use);
}

TEST(optional_empty)
{
	const char *pattern = "%[%]";
	char *expanded = ma_expand_custom(pattern, /*nmacros=*/0, /*macros=*/NULL,
			MA_OPT);
	assert_string_equal("", expanded);
	free(expanded);
}

TEST(optional_unmatched)
{
	check_hi("%[%1*a", /*nmacros=*/0, /*macros=*/NULL, MA_OPT,
			"%[a",
			"  1");
}

TEST(optional_with_empty_value)
{
	custom_macro_t macros[] = {
		{ .letter = 'n', .value = "" },
		{ .letter = 't', .value = "bla" },
	};

	const char *pattern = "before%[ in%nhere %]after";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros, MA_OPT);
	assert_string_equal("beforeafter", expanded);
	free(expanded);
}

TEST(optional_with_nonempty_value)
{
	custom_macro_t macros[] = {
		{ .letter = 'n', .value = "-" },
	};

	const char *pattern = "before%[ in%nhere %]after";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros, MA_OPT);
	assert_string_equal("before in-here after", expanded);
	free(expanded);
}

TEST(nested_optional_with_empty_value)
{
	custom_macro_t macros[] = {
		{ .letter = 'n', .value = "" },
	};

	const char *pattern = "before%[ %[ in%nhere %] %]after";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros, MA_OPT);
	assert_string_equal("beforeafter", expanded);
	free(expanded);
}

TEST(nested_optional_with_nonempty_value)
{
	custom_macro_t macros[] = {
		{ .letter = 'n', .value = "-" },
	};

	const char *pattern = "before%[ %[ in%nhere %] %]after";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros, MA_OPT);
	assert_string_equal("before  in-here  after", expanded);
	free(expanded);
}

TEST(mods_are_applied)
{
	custom_macro_t macros[] = {
		{ .letter = 'd', .value = "/a/b/<c>", .parent = "/", .expand_mods = 1 },
	};

	const char *pattern = "before%d:tafter";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros, MA_OPT);
	assert_string_equal("before<c>after", expanded);
	free(expanded);
}

TEST(empty_flag)
{
	custom_macro_t macros[] = {
		{ .letter = 'e', .value = "", .flag = 1 },
	};

	const char *pattern = "%[(empty)%e%]";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros, MA_OPT);
	assert_string_equal("", expanded);
	free(expanded);
}

TEST(non_empty_flag)
{
	custom_macro_t macros[] = {
		{ .letter = 'n', .value = "value", .flag = 1 },
	};

	const char *pattern = "%[(non-empty)%n%]";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros, MA_OPT);
	assert_string_equal("(non-empty)", expanded);
	free(expanded);
}

TEST(empty_flag_after_mod)
{
	custom_macro_t macros[] = {
		{ .letter = 'd', .value = "/a/b/<c>",
		  .parent = "/", .expand_mods = 1, .flag = 1 },
	};

	const char *pattern = "%[(empty)%d:t:e%]";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros, MA_OPT);
	assert_string_equal("", expanded);
	free(expanded);
}

TEST(non_empty_flag_after_mod)
{
	custom_macro_t macros[] = {
		{ .letter = 'd', .value = "/a/b/<c>",
		  .parent = "/", .expand_mods = 1, .flag = 1 },
	};

	const char *pattern = "%[(non-empty)%d:t%]";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros, MA_OPT);
	assert_string_equal("(non-empty)", expanded);
	free(expanded);
}

TEST(highlighting_is_ignored_by_uncolored_version)
{
	custom_macro_t macros[] = {
		{ .letter = 'i', .value = "xyz", .uses_left = 0, .group = -1, },
	};

	const char *pattern = "prefix %1*%i%2* suffix";
	char *expanded = ma_expand_custom(pattern, ARRAY_LEN(macros), macros, MA_OPT);
	assert_string_equal("prefix xyz suffix", expanded);
	free(expanded);
}

TEST(highlighting_is_set_correctly)
{
	custom_macro_t macros[] = {
		{ .letter = 'i', .value = "xyz", .uses_left = 0, .group = -1, },
	};
	check_hi("prefix %1*%i%* suffix", ARRAY_LEN(macros), macros, MA_NOOPT,
			"prefix xyz suffix",
			"       1  0      ");
}

TEST(opt_can_alter_highlighting_only)
{
	custom_macro_t macros[] = {
		{ .letter = 'C', .value = "", .flag = 1, },
	};
	check_hi("%1*%[%2*%C%]!", ARRAY_LEN(macros), macros, MA_OPT,
			"!",
			"1");

	macros[0].value = "*";
	check_hi("%1*%[%2*%C%]!", ARRAY_LEN(macros), macros, MA_OPT,
			"!",
			"2");
}

TEST(bad_user_group_remains_in_line_partially)
{
	custom_macro_t macros[1];
	check_hi("%10*", 0, macros, MA_NOOPT,
			"0*",
			"  ");
}

TEST(empty_optional_drops_attrs)
{
	custom_macro_t macros[1];
	check_hi("%1*%[%2*%]%3*", 0, macros, MA_OPT,
			"",
			"");
}

TEST(non_empty_optional_preserves_attrs)
{
	custom_macro_t macros[] = {
		{ .letter = 't', .value = "file", .uses_left = 0, .group = -1, },
	};

	check_hi("%1*%[%t%2*%t%]%3*", ARRAY_LEN(macros), macros, MA_OPT,
			"filefile",
			"1   2   ");

	check_hi("%1*%[%2*%t%3*%t%]%4*", ARRAY_LEN(macros), macros, MA_OPT,
			"filefile",
			"2   3   ");

	check_hi("%1*%[%2*%t%3*%]%4*", ARRAY_LEN(macros), macros, MA_OPT,
			"file",
			"2   ");
}

TEST(wide_characters_do_not_break_highlighting, IF(utf8_locale))
{
	custom_macro_t macros[1];
	check_hi("%1*螺丝 %= 螺%2*丝", 0, macros, MA_NOOPT,
			"螺丝  螺丝",
			"1       2 ");
}

static void
check_hi(const char pattern[], size_t nmacros, custom_macro_t macros[],
		int with_opt, const char line[], const char attrs[])
{
	cline_t expanded = ma_expand_colored_custom(pattern, nmacros, macros,
			with_opt);
	assert_string_equal(line, expanded.line);
	assert_string_equal(attrs, expanded.attrs);
	free(expanded.line);
	free(expanded.attrs);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
