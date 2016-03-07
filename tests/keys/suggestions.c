#include <stic.h>

#include <stddef.h> /* wchar_t */

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

static void process_suggestion(const wchar_t item[], const char descr[]);

static int nsuggestions;

SETUP()
{
	add_user_keys(L"hi", L"j", NORMAL_MODE, 0);
	add_user_keys(L"hi2", L"hi", NORMAL_MODE, 0);

	add_user_keys(L"ho", L"j", NORMAL_MODE, 0);
	add_user_keys(L"ha2", L"ho", NORMAL_MODE, 0);

	nsuggestions = 0;
}

TEST(all_keys_are_listed_no_selectors)
{
	vle_keys_suggest(L"", &process_suggestion);
	assert_int_equal(22, nsuggestions);
}

TEST(user_keys_with_prefix_are_listed)
{
	vle_keys_suggest(L"h", &process_suggestion);
	assert_int_equal(4, nsuggestions);
}

TEST(builtin_keys_with_prefix_are_listed)
{
	vle_keys_suggest(L"g", &process_suggestion);
	assert_int_equal(3, nsuggestions);
}

TEST(selectors_are_completed_from_beginning)
{
	vle_keys_suggest(L"d", &process_suggestion);
	assert_int_equal(8, nsuggestions);
}

TEST(selectors_are_completed_with_prefix)
{
	vle_keys_suggest(L"dg", &process_suggestion);
	assert_int_equal(1, nsuggestions);
}

static void
process_suggestion(const wchar_t item[], const char descr[])
{
	++nsuggestions;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
