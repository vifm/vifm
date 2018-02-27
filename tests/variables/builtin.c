#include <stic.h>

#include "../../src/engine/variables.h"

TEST(get_var_fails_for_unknown_varname)
{
	assert_true(getvar("v:test").type == VTYPE_ERROR);
}

TEST(get_var_works_for_known_varname)
{
	assert_success(setvar("v:test", var_from_bool(1)));
	assert_true(getvar("v:test").type == VTYPE_INT);
}

TEST(builtin_variables_are_read_only)
{
	assert_success(setvar("v:test", var_from_bool(0)));
	assert_failure(let_variables("v:test = 2"));
	assert_true(getvar("v:test").type == VTYPE_INT);
	assert_true(var_to_int(getvar("v:test")) == 0);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
