#include <stic.h>

#include "../../src/engine/parsing.h"
#include "../../src/engine/variables.h"
#include "../../src/engine/var.h"

#include "asserts.h"

SETUP()
{
	init_variables();
}

TEARDOWN()
{
	clear_variables();
}

TEST(nonexistent_variable_errors)
{
	ASSERT_FAIL("g:test", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("v:test", PE_INVALID_EXPRESSION);
}

TEST(existent_variable_is_expanded)
{
	assert_success(let_variables("g:test = 1"));
	ASSERT_OK("g:test", "1");
	assert_success(let_variables("g:test = 'abc'"));
	ASSERT_OK("g:test", "abc");

	assert_success(setvar("v:test", var_from_bool(1)));
	ASSERT_OK("v:test", "1");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
