#include <stic.h>

#include "../../src/engine/text_buffer.h"
#include "../../src/engine/variables.h"
#include "../../src/utils/env.h"

#define VAR_NAME_BASE "VAR"

SETUP()
{
	env_remove(VAR_NAME_BASE "1");
	env_remove(VAR_NAME_BASE "2");
	env_remove(VAR_NAME_BASE "3");
}

TEST(envvar_table_updates_do_not_crash)
{
	assert_int_equal(0, let_variables("$" VAR_NAME_BASE "1='VAL'"));
	assert_int_equal(0, unlet_variables("$" VAR_NAME_BASE "1"));
	assert_int_equal(0, let_variables("$" VAR_NAME_BASE "2='VAL'"));
	assert_int_equal(0, let_variables("$" VAR_NAME_BASE "3='VAL'"));
}

TEST(cannot_unlet_builtin_vars)
{
	assert_success(setvar("v:test", var_from_bool(1)));

	vle_tb_clear(vle_err);
	assert_failure(unlet_variables("v:test"));
	assert_string_equal("Unsupported variable type: v:test",
			vle_tb_get_data(vle_err));

	assert_true(getvar("v:test").type == VTYPE_INT);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
