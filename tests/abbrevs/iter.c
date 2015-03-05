#include <stic.h>

#include <stddef.h> /* NULL */

#include "../../src/engine/abbrevs.h"

TEST(nothing_to_iterate)
{
	void *state = NULL;
	const wchar_t *lhs, *rhs;
	assert_false(vle_abbr_iter(0, &lhs, &rhs, &state));
}

TEST(nulls_are_restored)
{
	void *state = NULL;
	const wchar_t *lhs, *rhs;

	assert_success(vle_abbr_add(L"lhs", L"rhs"));

	assert_true(vle_abbr_iter(0, &lhs, &rhs, &state));
	assert_non_null(lhs);
	assert_non_null(rhs);
	assert_non_null(state);

	assert_false(vle_abbr_iter(0, &lhs, &rhs, &state));
	assert_null(lhs);
	assert_null(rhs);
	assert_null(state);
}

TEST(repeated_iteration)
{
	void *state = NULL;
	const wchar_t *lhs, *rhs;

	assert_success(vle_abbr_add(L"lhs", L"rhs"));

	assert_true(vle_abbr_iter(0, &lhs, &rhs, &state));
	assert_false(vle_abbr_iter(0, &lhs, &rhs, &state));

	assert_true(vle_abbr_iter(0, &lhs, &rhs, &state));
	assert_false(vle_abbr_iter(0, &lhs, &rhs, &state));
}

TEST(single_element)
{
	void *state = NULL;
	const wchar_t *lhs, *rhs;

	assert_success(vle_abbr_add(L"lhs", L"rhs"));

	assert_true(vle_abbr_iter(0, &lhs, &rhs, &state));
	assert_wstring_equal(L"lhs", lhs);
	assert_wstring_equal(L"rhs", rhs);
	assert_false(vle_abbr_iter(0, &lhs, &rhs, &state));
}

TEST(several_elements)
{
	void *state = NULL;
	const wchar_t *lhs, *rhs;

	assert_success(vle_abbr_add(L"lhs1", L"rhs1"));
	assert_success(vle_abbr_add(L"lhs2", L"rhs2"));
	assert_success(vle_abbr_add(L"lhs3", L"rhs3"));

	assert_true(vle_abbr_iter(0, &lhs, &rhs, &state));
	assert_wstring_equal(L"lhs1", lhs);
	assert_wstring_equal(L"rhs1", rhs);

	assert_true(vle_abbr_iter(0, &lhs, &rhs, &state));
	assert_wstring_equal(L"lhs2", lhs);
	assert_wstring_equal(L"rhs2", rhs);

	assert_true(vle_abbr_iter(0, &lhs, &rhs, &state));
	assert_wstring_equal(L"lhs3", lhs);
	assert_wstring_equal(L"rhs3", rhs);

	assert_false(vle_abbr_iter(0, &lhs, &rhs, &state));
}

TEST(remap_only)
{
	void *state = NULL;
	const wchar_t *lhs, *rhs;

	assert_success(vle_abbr_add(L"lhs1", L"rhs1"));
	assert_success(vle_abbr_add_no_remap(L"lhs2", L"rhs2"));

	assert_true(vle_abbr_iter(0, &lhs, &rhs, &state));
	assert_wstring_equal(L"lhs1", lhs);
	assert_wstring_equal(L"rhs1", rhs);

	assert_false(vle_abbr_iter(0, &lhs, &rhs, &state));
}

TEST(no_remap_only)
{
	void *state = NULL;
	const wchar_t *lhs, *rhs;

	assert_success(vle_abbr_add(L"lhs1", L"rhs1"));
	assert_success(vle_abbr_add_no_remap(L"lhs2", L"rhs2"));

	assert_true(vle_abbr_iter(1, &lhs, &rhs, &state));
	assert_wstring_equal(L"lhs2", lhs);
	assert_wstring_equal(L"rhs2", rhs);

	assert_false(vle_abbr_iter(1, &lhs, &rhs, &state));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
