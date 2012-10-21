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

	init_undo_list_for_tests(&execute, &undo_levels);
}

static void
test_underflow(void)
{
	assert_int_equal(0, undo_group());
	assert_int_equal(0, undo_group());
	assert_int_equal(0, undo_group());
	assert_int_equal(-1, undo_group());

	assert_int_equal(4, i);
}

static void
test_redo(void)
{
	assert_int_equal(0, undo_group());
	assert_int_equal(0, undo_group());
	assert_int_equal(0, undo_group());
	assert_int_equal(0, redo_group());
	assert_int_equal(0, redo_group());
	assert_int_equal(0, redo_group());

	assert_int_equal(8, i);
}

static void
test_list_truncating(void)
{
	assert_int_equal(0, undo_group());
	assert_int_equal(0, undo_group());
	assert_int_equal(0, undo_group());
	assert_int_equal(0, redo_group());
	assert_int_equal(0, redo_group());
	assert_int_equal(0, redo_group());
	assert_int_equal(0, undo_group());
	assert_int_equal(0, undo_group());

	cmd_group_begin("msg4");
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg4",
			"undo_msg4"));
	cmd_group_end();

	assert_int_equal(-1, redo_group());

	assert_int_equal(11, i);
}

static void
test_cmd_1undo_1redo(void)
{
	static int undo_levels = 10;

	reset_undo_list();
	init_undo_list_for_tests(&exec_dummy, &undo_levels);

	cmd_group_begin("msg0");
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg0",
			"undo_msg0"));
	cmd_group_end();

	assert_int_equal(0, undo_group());
	assert_int_equal(0, redo_group());
}

static int
execute_fail(OPS op, void *data, const char *src, const char *dst)
{
	if(op == OP_NONE)
		return 0;
	return !strcmp(src, "undo_msg0");
}

static void
test_failed_operation(void)
{
	static int undo_levels = 10;

	init_undo_list_for_tests(&execute_fail, &undo_levels);
	reset_undo_list();

	cmd_group_begin("msg0");
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg0",
			"undo_msg0"));
	cmd_group_end();

	cmd_group_begin("msg1");
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg1",
			"undo_msg1"));
	cmd_group_end();

	assert_int_equal(0, undo_group());
	assert_int_equal(-2, undo_group());
	assert_int_equal(-1, undo_group());
	assert_int_equal(1, redo_group());
	assert_int_equal(0, redo_group());
	assert_int_equal(-1, redo_group());
}

static void
test_disbalance(void)
{
	static int undo_levels = 10;

	init_undo_list_for_tests(&execute_fail, &undo_levels);
	reset_undo_list();

	cmd_group_begin("msg0");
	assert_int_equal(0, add_operation(OP_COPY, NULL, NULL, "do_msg0",
			"undo_msg0"));
	cmd_group_end();

	cmd_group_begin("msg1");
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg1",
			"undo_msg1"));
	cmd_group_end();

	assert_int_equal(0, undo_group());
	assert_int_equal(-3, undo_group());
	assert_int_equal(-1, undo_group());
	assert_int_equal(-4, redo_group());
	assert_int_equal(0, redo_group());
	assert_int_equal(-1, redo_group());
}

static void
test_cannot_be_undone(void)
{
	cmd_group_begin("msg0");
	assert_int_equal(0, add_operation(OP_REMOVE, NULL, NULL, "do_msg0", ""));
	cmd_group_end();

	assert_int_equal(-5, undo_group());
}

static void
test_removing_of_incomplete_groups(void)
{
	static int undo_levels = 10;
	int i;

	init_undo_list_for_tests(&exec_dummy, &undo_levels);

	cmd_group_begin("msg0");
	for(i = 0; i < 10; i++)
		assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg0",
				"undo_msg0"));
	cmd_group_end();

	cmd_group_begin("msg1");
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg1",
			"undo_msg1"));
	cmd_group_end();

	assert_int_equal(0, undo_group());
	assert_int_equal(-1, undo_group());
}

static int
exec_skip(OPS op, void *data, const char *src, const char *dst)
{
	return SKIP_UNDO_REDO_OPERATION;
}

static void
test_skipping(void)
{
	static int undo_levels = 10;
	init_undo_list_for_tests(&exec_skip, &undo_levels);

	cmd_group_begin("msg0");
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg0",
			"undo_msg0"));
	cmd_group_end();

	assert_int_equal(-6, undo_group());
	assert_int_equal(-4, redo_group());
}

static void
test_to_many_commands_and_continue(void)
{
	static int undo_levels = 3;
	init_undo_list_for_tests(&exec_skip, &undo_levels);

	cmd_group_begin("msg0");
	cmd_group_end();

	cmd_group_continue();
	free(replace_group_msg("HI"));
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg0",
			"undo_msg0"));
	cmd_group_end();
	cmd_group_continue();
	free(replace_group_msg("HI"));
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg0",
			"undo_msg0"));
	cmd_group_end();
	cmd_group_continue();
	free(replace_group_msg("HI"));
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg0",
			"undo_msg0"));
	cmd_group_end();
	cmd_group_continue();
	free(replace_group_msg("HI"));
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg0",
			"undo_msg0"));
	cmd_group_end();
	cmd_group_continue();
	free(replace_group_msg("HI"));
	assert_int_equal(0, add_operation(OP_MOVE, NULL, NULL, "do_msg0",
			"undo_msg0"));
	cmd_group_end();
}

void
undo_test(void)
{
	test_fixture_start();

	fixture_setup(setup);

	run_test(test_underflow);
	run_test(test_redo);
	run_test(test_list_truncating);
	run_test(test_cmd_1undo_1redo);
	run_test(test_failed_operation);
	run_test(test_disbalance);
	run_test(test_cannot_be_undone);
	run_test(test_removing_of_incomplete_groups);
	run_test(test_skipping);
	run_test(test_to_many_commands_and_continue);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
