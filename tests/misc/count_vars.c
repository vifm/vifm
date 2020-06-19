#include <stic.h>

#include <string.h> /* strdup() */

#include <test-utils.h>

#include "../../src/engine/keys.h"
#include "../../src/engine/variables.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
#include "../../src/cmd_core.h"

SETUP()
{
	curr_view = &lwin;

	view_setup(&lwin);
	init_commands();
	init_modes();
	opt_handlers_setup();
}

TEARDOWN()
{
	view_teardown(&lwin);
	vle_keys_reset();
	opt_handlers_teardown();
}

TEST(count_vars_are_set_to_defaults_normal)
{
	var_t var;

	(void)vle_keys_exec_timed_out(L":");

	var = getvar("v:count");
	assert_int_equal(VTYPE_INT, var.type);
	assert_int_equal(0, var_to_int(var));

	var = getvar("v:count1");
	assert_int_equal(VTYPE_INT, var.type);
	assert_int_equal(1, var_to_int(var));

	(void)vle_keys_exec_timed_out(WK_ESC);
}

TEST(count_vars_are_set_to_passed_value_normal)
{
	var_t var;

	(void)vle_keys_exec_timed_out(L"3:");

	var = getvar("v:count");
	assert_int_equal(VTYPE_INT, var.type);
	assert_int_equal(3, var_to_int(var));

	var = getvar("v:count1");
	assert_int_equal(VTYPE_INT, var.type);
	assert_int_equal(3, var_to_int(var));

	(void)vle_keys_exec_timed_out(WK_ESC);
}

TEST(count_vars_are_set_to_passed_value_visual)
{
	var_t var;

	lwin.list_rows = 1;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("lfile0");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];

	(void)vle_keys_exec_timed_out(L"v1:");

	var = getvar("v:count");
	assert_int_equal(VTYPE_INT, var.type);
	assert_int_equal(1, var_to_int(var));

	var = getvar("v:count1");
	assert_int_equal(VTYPE_INT, var.type);
	assert_int_equal(1, var_to_int(var));

	(void)vle_keys_exec_timed_out(WK_ESC);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
