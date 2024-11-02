#include <stic.h>

#include "../../src/engine/variables.h"

TEST(get_var_fails_for_unknown_global)
{
	assert_true(getvar("g:test").type == VTYPE_ERROR);
}

TEST(get_var_works_for_known_global)
{
	assert_success(let_variables("g:test = 1"));
	assert_true(getvar("g:test").type == VTYPE_INT);
}

TEST(globals_can_be_removed)
{
	assert_success(let_variables("g:test1 = 'a'"));
	assert_success(let_variables("g:test2 = 0"));
	assert_success(unlet_variables("g:test1 g:test2"));
	assert_true(getvar("g:test1").type == VTYPE_ERROR);
	assert_true(getvar("g:test2").type == VTYPE_ERROR);
}

TEST(globals_can_be_strings)
{
	assert_success(let_variables("g:test = 'abc'"));
	assert_true(getvar("g:test").type == VTYPE_STRING);
}

TEST(globals_can_change_value)
{
	assert_success(let_variables("g:test = 0"));
	assert_int_equal(0, var_to_int(getvar("g:test")));

	assert_success(let_variables("g:test = 2"));
	assert_int_equal(2, var_to_int(getvar("g:test")));
}

TEST(globals_can_change_type)
{
	assert_success(let_variables("g:test = 0"));
	assert_true(getvar("g:test").type == VTYPE_INT);
	assert_int_equal(0, var_to_int(getvar("g:test")));

	assert_success(let_variables("g:test = 'abc'"));
	assert_true(getvar("g:test").type == VTYPE_STRING);
	assert_string_equal("abc", getvar("g:test").value.string);
}

TEST(int_globals_can_be_incremented_or_decremented)
{
	assert_success(let_variables("g:test = 7"));
	assert_int_equal(7, var_to_int(getvar("g:test")));

	assert_success(let_variables("g:test += 2"));
	assert_int_equal(9, var_to_int(getvar("g:test")));

	assert_success(let_variables("g:test -= 10"));
	assert_int_equal(-1, var_to_int(getvar("g:test")));
}

TEST(string_globals_cannot_be_incremented_or_decremented)
{
	assert_success(let_variables("g:test = 'a'"));

	assert_failure(let_variables("g:test += 'b'"));
	assert_string_equal("a", getvar("g:test").value.string);

	assert_failure(let_variables("g:test -= 'a'"));
	assert_string_equal("a", getvar("g:test").value.string);
}

TEST(int_globals_cannot_be_appended)
{
	assert_success(let_variables("g:test = 0"));

	assert_failure(let_variables("g:test .= 1"));
	assert_int_equal(0, var_to_int(getvar("g:test")));
	assert_true(getvar("g:test").type == VTYPE_INT);

	assert_failure(let_variables("g:test .= '1'"));
	assert_int_equal(0, var_to_int(getvar("g:test")));
	assert_true(getvar("g:test").type == VTYPE_INT);
}

TEST(string_globals_can_be_appended)
{
	assert_success(let_variables("g:test = 'a'"));
	assert_success(let_variables("g:test .= 'b'"));
	assert_string_equal("ab", getvar("g:test").value.string);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
