#include <stic.h>

#include <stddef.h> /* NULL wchar_t */

#include "../../src/engine/abbrevs.h"
#include "../../src/utils/str.h"

static wchar_t * handler(void *user_data);

TEST(delete_inexistent)
{
	assert_false(vle_abbr_remove(L"lhs") == 0);
}

TEST(delete_existent_by_lhs)
{
	assert_success(vle_abbr_add(L"lhs", L"rhs"));
	assert_success(vle_abbr_remove(L"lhs"));

	assert_success(vle_abbr_add_no_remap(L"lhs", L"rhs"));
	assert_success(vle_abbr_remove(L"lhs"));
}

TEST(delete_existent_by_rhs)
{
	assert_success(vle_abbr_add(L"lhs", L"rhs"));
	assert_success(vle_abbr_remove(L"rhs"));

	assert_success(vle_abbr_add_no_remap(L"lhs", L"rhs"));
	assert_success(vle_abbr_remove(L"rhs"));
}

TEST(delete_existent_by_rhs_check_all_lhs_first)
{
	assert_success(vle_abbr_add(L"a", L"b"));
	assert_success(vle_abbr_add_no_remap(L"b", L"c"));

	assert_success(vle_abbr_remove(L"b"));
	assert_success(vle_abbr_remove(L"a"));
}

TEST(delete_foreign_by_lhs)
{
	assert_success(vle_abbr_add_foreign(L"lhs", "descr", /*no_remap=*/1, handler,
				/*user_data=*/NULL));
	assert_success(vle_abbr_remove(L"lhs"));
}

TEST(delete_foreign_by_rhs)
{
	int no_remap;

	assert_success(vle_abbr_add_foreign(L"lhs", "descr", /*no_remap=*/1, handler,
				/*user_data=*/NULL));
	assert_failure(vle_abbr_remove(L"hrhs"));
	(void)vle_abbr_expand(L"lhs", &no_remap);
	assert_success(vle_abbr_remove(L"hrhs"));
}

static wchar_t *
handler(void *user_data)
{
	return vifm_wcsdup(L"hrhs");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
