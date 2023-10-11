#include <stic.h>

#include <stddef.h> /* NULL */

#include "../../src/engine/abbrevs.h"

TEST(nothing_to_iterate)
{
	void *state = NULL;
	const wchar_t *lhs, *rhs;
	const char *descr;
	int no_remap = -1;
	assert_false(vle_abbr_iter(&lhs, &rhs, &descr, &no_remap, &state));
}

TEST(nulls_are_restored)
{
	void *state = NULL;
	const wchar_t *lhs, *rhs;
	const char *descr;
	int no_remap;

	assert_success(vle_abbr_add(L"lhs", L"rhs"));

	assert_true(vle_abbr_iter(&lhs, &rhs, &descr, &no_remap, &state));
	assert_non_null(lhs);
	assert_non_null(rhs);
	assert_null(descr);
	assert_non_null(state);

	assert_false(vle_abbr_iter(&lhs, &rhs, &descr, &no_remap, &state));
	assert_null(lhs);
	assert_null(rhs);
	assert_null(descr);
	assert_null(state);
}

TEST(repeated_iteration)
{
	void *state = NULL;
	const wchar_t *lhs, *rhs;
	const char *descr;
	int no_remap;

	assert_success(vle_abbr_add(L"lhs", L"rhs"));

	assert_true(vle_abbr_iter(&lhs, &rhs, &descr, &no_remap, &state));
	assert_false(vle_abbr_iter(&lhs, &rhs, &descr, &no_remap, &state));

	assert_true(vle_abbr_iter(&lhs, &rhs, &descr, &no_remap, &state));
	assert_false(vle_abbr_iter(&lhs, &rhs, &descr, &no_remap, &state));
}

TEST(single_element)
{
	void *state = NULL;
	const wchar_t *lhs, *rhs;
	const char *descr;
	int no_remap = -1;

	assert_success(vle_abbr_add(L"lhs", L"rhs"));

	assert_true(vle_abbr_iter(&lhs, &rhs, &descr, &no_remap, &state));
	assert_wstring_equal(L"lhs", lhs);
	assert_wstring_equal(L"rhs", rhs);
	assert_null(descr);
	assert_false(no_remap);
	assert_false(vle_abbr_iter(&lhs, &rhs, &descr, &no_remap, &state));
}

TEST(single_foreign_element)
{
	void *state = NULL;
	const wchar_t *lhs, *rhs;
	const char *descr;
	int no_remap = -1;

	assert_success(vle_abbr_add_foreign(L"lhs", "descr", /*no_remap=*/1,
				/*handler=*/NULL, /*user_data=*/NULL));

	assert_true(vle_abbr_iter(&lhs, &rhs, &descr, &no_remap, &state));
	assert_wstring_equal(L"lhs", lhs);
	assert_null(rhs);
	assert_string_equal("descr", descr);
	assert_true(no_remap);
	assert_false(vle_abbr_iter(&lhs, &rhs, &descr, &no_remap, &state));
}

TEST(several_elements)
{
	void *state = NULL;
	const wchar_t *lhs, *rhs;
	const char *descr;
	int no_remap = -1;

	assert_success(vle_abbr_add(L"lhs1", L"rhs1"));
	assert_success(vle_abbr_add_no_remap(L"lhs2", L"rhs2"));
	assert_success(vle_abbr_add_foreign(L"lhs3", "descr3", /*no_remap=*/0,
				/*handler=*/NULL, /*user_data=*/NULL));
	assert_success(vle_abbr_add_foreign(L"lhs4", "descr4", /*no_remap=*/1,
				/*handler=*/NULL, /*user_data=*/NULL));

	assert_true(vle_abbr_iter(&lhs, &rhs, &descr, &no_remap, &state));
	assert_wstring_equal(L"lhs1", lhs);
	assert_wstring_equal(L"rhs1", rhs);
	assert_null(descr);
	assert_false(no_remap);

	assert_true(vle_abbr_iter(&lhs, &rhs, &descr, &no_remap, &state));
	assert_wstring_equal(L"lhs2", lhs);
	assert_wstring_equal(L"rhs2", rhs);
	assert_null(descr);
	assert_true(no_remap);

	assert_true(vle_abbr_iter(&lhs, &rhs, &descr, &no_remap, &state));
	assert_wstring_equal(L"lhs3", lhs);
	assert_null(rhs);
	assert_string_equal("descr3", descr);
	assert_false(no_remap);

	assert_true(vle_abbr_iter(&lhs, &rhs, &descr, &no_remap, &state));
	assert_wstring_equal(L"lhs4", lhs);
	assert_null(rhs);
	assert_string_equal("descr4", descr);
	assert_true(no_remap);

	assert_false(vle_abbr_iter(&lhs, &rhs, &descr, &no_remap, &state));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
