#include <stdlib.h>

#include "seatest.h"

#include "../../src/undo.h"

static void
execute_dummy(const char *cmd)
{
}

static void
test_undolevel_change_to_smaller(void)
{
	static int undo_levels = 3;

	init_undo_list(&execute_dummy, &undo_levels);

	cmd_group_begin("msg4");
	assert_int_equal(0, add_operation("do_msg4", "undo_msg4"));
	cmd_group_end();

	assert_int_equal(0, undo_group());
	assert_int_equal(0, undo_group());
	assert_int_equal(0, undo_group());
	assert_int_equal(-1, undo_group());
}

static void
test_zero_undolevel(void)
{
	static int undo_levels = 0;

	init_undo_list(&execute_dummy, &undo_levels);

	cmd_group_begin("msg4");
	assert_int_equal(0, add_operation("do_msg4", "undo_msg4"));
	cmd_group_end();

	assert_int_equal(-1, undo_group());
}

static void
test_negative_undolevel(void)
{
	static int undo_levels = -1;

	init_undo_list(&execute_dummy, &undo_levels);

	cmd_group_begin("msg4");
	assert_int_equal(0, add_operation("do_msg4", "undo_msg4"));
	cmd_group_end();

	assert_int_equal(-1, undo_group());
}

void
undolevels_test(void)
{
	test_fixture_start();

	run_test(test_undolevel_change_to_smaller);
	run_test(test_zero_undolevel);
	run_test(test_negative_undolevel);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
