#include <stic.h>

#include <stdlib.h>
#include <string.h> /* strcmp() */

#include "../../src/ops.h"
#include "../../src/undo.h"

#include "test.h"

static int execute(OPS op, void *data, const char *src, const char *dst);

static int i;

SETUP()
{
	static int undo_levels = 10;

	i = 0;

	init_undo_list_for_tests(&execute, &undo_levels);
}

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

TEST(underflow)
{
	assert_int_equal(0, undo_group());
	assert_int_equal(0, undo_group());
	assert_int_equal(0, undo_group());
	assert_int_equal(-1, undo_group());

	assert_int_equal(4, i);
}

TEST(redo)
{
	assert_int_equal(0, undo_group());
	assert_int_equal(0, undo_group());
	assert_int_equal(0, undo_group());
	assert_int_equal(0, redo_group());
	assert_int_equal(0, redo_group());
	assert_int_equal(0, redo_group());

	assert_int_equal(8, i);
}

TEST(list_truncating)
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

TEST(cmd_1undo_1redo)
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

TEST(failed_operation)
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

TEST(disbalance)
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

TEST(cannot_be_undone)
{
	cmd_group_begin("msg0");
	assert_int_equal(0, add_operation(OP_REMOVE, NULL, NULL, "do_msg0", ""));
	cmd_group_end();

	assert_int_equal(-5, undo_group());
}

TEST(removing_of_incomplete_groups)
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

TEST(skipping)
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

TEST(to_many_commands_and_continue)
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
