#include <stic.h>

#include <stddef.h> /* wchar_t */

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

#include "suite.h"

static void process_listing(const wchar_t lhs[], const wchar_t rhs[],
		const char descr[]);

static int nitems;

SETUP()
{
	assert_success(set_user_key(L"hi", L"j", NORMAL_MODE));
	assert_success(set_user_key(L"hi2", L"hi", NORMAL_MODE));

	assert_success(set_user_key(L"ho", L"j", NORMAL_MODE));
	assert_success(set_user_key(L"ha2", L"ho", NORMAL_MODE));

	nitems = 0;
}

TEST(normal_mode_keys)
{
	vle_keys_list(NORMAL_MODE, &process_listing, 0);
	assert_int_equal(1 + /*user=*/4 + 2 + /*builtin=*/19, nitems);

	nitems = 0;
	vle_keys_list(NORMAL_MODE, &process_listing, 1);
	assert_int_equal(1 + /*user=*/4 + 2, nitems);
}

TEST(visual_mode_keys)
{
	vle_keys_list(VISUAL_MODE, &process_listing, 0);
	assert_int_equal(1 + 2 + /*builtin=*/4, nitems);
}

static void
process_listing(const wchar_t lhs[], const wchar_t rhs[], const char descr[])
{
	++nitems;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
