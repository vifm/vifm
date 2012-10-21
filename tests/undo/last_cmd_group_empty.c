#include <stdlib.h>

#include "seatest.h"

#include "../../src/ops.h"
#include "../../src/undo.h"

#include "test.h"

static int i;

static int
execute(OPS op, void *data, const char *src, const char *dst)
{
	static const char *execs[] = {
		"undo_msg3",
		"undo_msg2_cmd2",
		"undo_msg2_cmd1",
		"undo_msg1",
		"do_msg1",
		"do_msg2_cmd1",
		"do_msg2_cmd2",
		"do_msg3",
		"undo_msg3",
		"undo_msg2_cmd2",
		"undo_msg2_cmd1",
	};

	if(op == OP_NONE)
		return 0;
	assert_string_equal(execs[i++], src);
	return 0;
}

static int
exec_dummy(OPS op, void *data, const char *src, const char *dst)
{
	return 0;
}

static void
setup(void)
{
	static int undo_levels = 10;

	i = 0;

	reset_undo_list();
	init_undo_list_for_tests(&execute, &undo_levels);
}

static void
test_empty_undo_list_true(void)
{
	assert_true(last_cmd_group_empty());
}

static void
test_empty_group_true(void)
{
	cmd_group_begin("msg4");
	cmd_group_end();
	assert_true(last_cmd_group_empty());
}

static void
test_non_empty_group_false(void)
{
	cmd_group_begin("msg0");
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg0",
			"undo_msg0"));
	cmd_group_end();
	assert_false(last_cmd_group_empty());
}

static void
test_before_add_in_group_true(void)
{
	cmd_group_begin("msg0");
	assert_true(last_cmd_group_empty());
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg0",
			"undo_msg0"));
	cmd_group_end();
}

static void
test_after_add_in_group_false(void)
{
	cmd_group_begin("msg0");
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg0",
			"undo_msg0"));
	assert_false(last_cmd_group_empty());
	cmd_group_end();
}

void
last_cmd_group_empty_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);

	run_test(test_empty_undo_list_true);
	run_test(test_empty_group_true);
	run_test(test_non_empty_group_false);
	run_test(test_before_add_in_group_true);
	run_test(test_after_add_in_group_false);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
