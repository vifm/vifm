#include <stic.h>

#include <stddef.h> /* NULL wchar_t */
#include <wchar.h> /* wcscmp() */

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

#include "suite.h"

static void process_suggestion(const wchar_t lhs[], const wchar_t rhs[],
		const char descr[]);

static int nsuggestions;
static const wchar_t *rhs;
static const char *descr;

SETUP()
{
	assert_success(set_user_key(L"hi", L"j", NORMAL_MODE));
	assert_success(set_user_key(L"hi2", L"hi", NORMAL_MODE));

	assert_success(set_user_key(L"ho", L"j", NORMAL_MODE));
	assert_success(set_user_key(L"ha2", L"ho", NORMAL_MODE));

	nsuggestions = 0;
	rhs = NULL;
	descr = NULL;
}

TEST(all_keys_are_listed_no_selectors_no_folding)
{
	vle_keys_suggest(L"", &process_suggestion, 0, 0);
	assert_int_equal(22, nsuggestions);
}

TEST(all_keys_are_listed_no_selectors_folding)
{
	vle_keys_suggest(L"", &process_suggestion, 0, 1);
	assert_int_equal(16, nsuggestions);
}

TEST(user_keys_with_prefix_are_listed_no_folding)
{
	vle_keys_suggest(L"h", &process_suggestion, 0, 0);
	assert_int_equal(4, nsuggestions);
}

TEST(user_keys_with_prefix_are_listed_folding)
{
	assert_success(set_user_key(L"ha3", L"a", NORMAL_MODE));

	vle_keys_suggest(L"h", &process_suggestion, 0, 1);
	assert_int_equal(4, nsuggestions);
}

TEST(builtin_keys_with_prefix_are_listed)
{
	vle_keys_suggest(L"g", &process_suggestion, 0, 0);
	assert_int_equal(3, nsuggestions);
}

TEST(selectors_are_completed_from_beginning)
{
	vle_keys_suggest(L"d", &process_suggestion, 0, 0);
	assert_int_equal(7, nsuggestions);
}

TEST(selectors_are_completed_with_prefix)
{
	vle_keys_suggest(L"dg", &process_suggestion, 0, 0);
	assert_int_equal(1, nsuggestions);
}

TEST(descr_of_user_defined_keys_is_rhs)
{
	vle_keys_suggest(L"ha", &process_suggestion, 0, 0);
	assert_int_equal(1, nsuggestions);
	assert_int_equal(0, wcscmp(rhs, L"ho"));
	assert_string_equal("", descr);
}

TEST(custom_suggestions)
{
	vle_keys_suggest(L"m", &process_suggestion, 0, 0);
	assert_int_equal(2, nsuggestions);
}

TEST(only_custom_suggestions)
{
	vle_keys_suggest(L"", &process_suggestion, 1, 0);
	assert_int_equal(0, nsuggestions);

	vle_keys_suggest(L"m", &process_suggestion, 1, 0);
	assert_int_equal(2, nsuggestions);
}

TEST(suggestions_after_user_mapped_user_prefix)
{
	assert_success(set_user_key(L"x", L"h", NORMAL_MODE));

	vle_keys_suggest(L"x", &process_suggestion, 0, 0);
	assert_int_equal(4, nsuggestions);
	assert_wstring_equal(L"j", rhs);
	assert_string_equal("", descr);
}

TEST(suggestions_after_user_mapped_builtin_prefix)
{
	assert_success(set_user_key(L"x", L"g", NORMAL_MODE));

	vle_keys_suggest(L"x", &process_suggestion, 0, 0);
	assert_int_equal(3, nsuggestions);
	assert_wstring_equal(L"", rhs);
	assert_string_equal("", descr);
}

TEST(suggestions_after_user_noremapped_builtin_prefix)
{
	assert_success(vle_keys_user_add(L"x", L"g", "descr", NORMAL_MODE,
				KEYS_FLAG_NOREMAP));

	vle_keys_suggest(L"x", &process_suggestion, 0, 0);
	assert_int_equal(3, nsuggestions);
	assert_wstring_equal(L"", rhs);
	assert_string_equal("", descr);
}

TEST(suggestions_on_common_selector_or_command_prefix)
{
	/* We should get both "gugu" and "gugg". */
	vle_keys_suggest(L"gug", &process_suggestion, 0, 0);
	assert_int_equal(2, nsuggestions);
	assert_wstring_equal(L"", rhs);
	assert_string_equal("", descr);
}

TEST(user_help_message_is_used_as_a_description)
{
	assert_success(vle_keys_user_add(L"hx", L"G", "hx help", NORMAL_MODE,
				KEYS_FLAG_NONE));

	vle_keys_suggest(L"h", &process_suggestion, 0, 0);
	assert_int_equal(5, nsuggestions);
	assert_wstring_equal(L"G", rhs);
	assert_string_equal("hx help", descr);
}

static void
process_suggestion(const wchar_t lhs[], const wchar_t r[], const char d[])
{
	++nsuggestions;
	rhs = r;
	descr = d;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
