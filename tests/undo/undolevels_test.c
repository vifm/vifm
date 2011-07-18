#include <stdlib.h>

#include "seatest.h"

#include "../../src/undo.h"

static int
execute_dummy(const char *cmd)
{
	return 0;
}

static void
test_undolevel_change_to_smaller(void)
{
	static int undo_levels = 4;

	init_undo_list(&execute_dummy, &undo_levels);

	cmd_group_begin("msg4");
	assert_int_equal(0, add_operation("do_msg4", NULL, NULL, "undo_msg4", NULL,
			NULL));
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
	assert_int_equal(0, add_operation("do_msg4", NULL, NULL, "undo_msg4", NULL,
			NULL));
	cmd_group_end();

	assert_int_equal(-1, undo_group());
}

static void
test_negative_undolevel(void)
{
	static int undo_levels = -1;

	init_undo_list(&execute_dummy, &undo_levels);

	cmd_group_begin("msg4");
	assert_int_equal(0, add_operation("do_msg4", NULL, NULL, "undo_msg4", NULL,
			NULL));
	cmd_group_end();

	assert_int_equal(-1, undo_group());
}

static void
test_too_many_commands(void)
{
	cmd_group_begin("msg4");
	assert_int_equal(0, add_operation("do_msg40", NULL, NULL, "undo_msg40", NULL,
			NULL));
	assert_int_equal(0, add_operation("do_msg41", NULL, NULL, "undo_msg41", NULL,
			NULL));
	assert_int_equal(0, add_operation("do_msg42", NULL, NULL, "undo_msg42", NULL,
			NULL));
	assert_int_equal(0, add_operation("do_msg43", NULL, NULL, "undo_msg43", NULL,
			NULL));
	assert_int_equal(0, add_operation("do_msg44", NULL, NULL, "undo_msg44", NULL,
			NULL));
	assert_int_equal(0, add_operation("do_msg45", NULL, NULL, "undo_msg45", NULL,
			NULL));
	assert_int_equal(0, add_operation("do_msg46", NULL, NULL, "undo_msg46", NULL,
			NULL));
	assert_int_equal(0, add_operation("do_msg47", NULL, NULL, "undo_msg47", NULL,
			NULL));
	assert_int_equal(0, add_operation("do_msg48", NULL, NULL, "undo_msg48", NULL,
			NULL));
	assert_int_equal(0, add_operation("do_msg49", NULL, NULL, "undo_msg49", NULL,
			NULL));
	assert_int_equal(0, add_operation("do_msg4a", NULL, NULL, "undo_msg4a", NULL,
			NULL));
	assert_int_equal(0, add_operation("do_msg4b", NULL, NULL, "undo_msg4b", NULL,
			NULL));
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
