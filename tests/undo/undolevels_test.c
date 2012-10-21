#include <stdlib.h>

#include "seatest.h"

#include "../../src/undo.h"

#include "test.h"

static int
execute_dummy(OPS op, void *data, const char *src, const char *dst)
{
	return 0;
}

static void
test_undolevel_change_to_smaller(void)
{
	static int undo_levels = 4;

	init_undo_list_for_tests(&execute_dummy, &undo_levels);

	cmd_group_begin("msg4");
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg4",
			"undo_msg4"));
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

	init_undo_list_for_tests(&execute_dummy, &undo_levels);

	cmd_group_begin("msg4");
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg4",
			"undo_msg4"));
	cmd_group_end();

	assert_int_equal(-1, undo_group());
}

static void
test_negative_undolevel(void)
{
	static int undo_levels = -1;

	init_undo_list_for_tests(&execute_dummy, &undo_levels);

	cmd_group_begin("msg4");
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg4",
			"undo_msg4"));
	cmd_group_end();

	assert_int_equal(-1, undo_group());
}

static void
test_too_many_commands(void)
{
	cmd_group_begin("msg4");
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg40",
			"undo_msg40"));
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg41",
			"undo_msg41"));
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg42",
				"undo_msg42"));
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg43",
				"undo_msg43"));
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg44",
				"undo_msg44"));
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg45",
				"undo_msg45"));
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg46",
				"undo_msg46"));
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg47",
				"undo_msg47"));
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg48",
				"undo_msg48"));
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg49",
				"undo_msg49"));
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg4a",
				"undo_msg4a"));
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg4b",
				"undo_msg4b"));
	cmd_group_end();
}

void
undolevels_test(void)
{
	test_fixture_start();

	run_test(test_undolevel_change_to_smaller);
	run_test(test_zero_undolevel);
	run_test(test_negative_undolevel);
	run_test(test_too_many_commands);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
