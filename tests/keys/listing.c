#include <stic.h>

#include <stddef.h> /* wchar_t */

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

static void process_listing(const wchar_t lhs[], const wchar_t rhs[]);

static int nitems;

SETUP()
{
	add_user_keys(L"hi", L"j", NORMAL_MODE, 0);
	add_user_keys(L"hi2", L"hi", NORMAL_MODE, 0);

	add_user_keys(L"ho", L"j", NORMAL_MODE, 0);
	add_user_keys(L"ha2", L"ho", NORMAL_MODE, 0);

	nitems = 0;
}

TEST(normal_mode_user_keys_delimiter)
{
	list_cmds(NORMAL_MODE, &process_listing);
	assert_int_equal(21, nitems);
}

TEST(visual_mode_no_user_keys_no_delimiter)
{
	list_cmds(VISUAL_MODE, &process_listing);
	assert_int_equal(4, nitems);
}

static void
process_listing(const wchar_t lhs[], const wchar_t rhs[])
{
	++nitems;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
