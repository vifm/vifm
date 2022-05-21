#include <stic.h>

#include <stdlib.h>
#include <string.h> /* strcmp() */

#include "../../src/ops.h"
#include "../../src/undo.h"

#include "test.h"

static OpsResult execute(OPS op, void *data, const char src[],
		const char dst[]);

static int i;

SETUP()
{
	static int undo_levels = 10;

	i = 0;

	init_undo_list_for_tests(&execute, &undo_levels);
}

static OpsResult
execute(OPS op, void *data, const char src[], const char dst[])
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
		return OPS_SUCCEEDED;
	assert_string_equal(execs[i++], src);
	return OPS_SUCCEEDED;
}

static OpsResult
exec_dummy(OPS op, void *data, const char src[], const char dst[])
{
	return OPS_SUCCEEDED;
}

TEST(underflow)
{
	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	assert_int_equal(UN_ERR_NONE, un_group_undo());

	assert_int_equal(4, i);
}

TEST(redo)
{
	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	assert_int_equal(UN_ERR_SUCCESS, un_group_redo());
	assert_int_equal(UN_ERR_SUCCESS, un_group_redo());
	assert_int_equal(UN_ERR_SUCCESS, un_group_redo());

	assert_int_equal(8, i);
}

TEST(list_truncating)
{
	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	assert_int_equal(UN_ERR_SUCCESS, un_group_redo());
	assert_int_equal(UN_ERR_SUCCESS, un_group_redo());
	assert_int_equal(UN_ERR_SUCCESS, un_group_redo());
	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());

	un_group_open("msg4");
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg4",
			"undo_msg4"));
	un_group_close();

	assert_int_equal(UN_ERR_NONE, un_group_redo());

	assert_int_equal(11, i);
}

TEST(cmd_1undo_1redo)
{
	static int undo_levels = 10;

	un_reset();
	init_undo_list_for_tests(&exec_dummy, &undo_levels);

	un_group_open("msg0");
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg0",
			"undo_msg0"));
	un_group_close();

	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	assert_int_equal(UN_ERR_SUCCESS, un_group_redo());
}

static OpsResult
execute_fail(OPS op, void *data, const char *src, const char *dst)
{
	if(op == OP_NONE)
		return OPS_SUCCEEDED;
	return (strcmp(src, "undo_msg0") == 0 ? OPS_FAILED : OPS_SUCCEEDED);
}

TEST(failed_operation)
{
	static int undo_levels = 10;

	init_undo_list_for_tests(&execute_fail, &undo_levels);
	un_reset();

	un_group_open("msg0");
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg0",
			"undo_msg0"));
	un_group_close();

	un_group_open("msg1");
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg1",
			"undo_msg1"));
	un_group_close();

	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	assert_int_equal(UN_ERR_FAIL, un_group_undo());
	assert_int_equal(UN_ERR_NONE, un_group_undo());
	assert_int_equal(UN_ERR_ERRORS, un_group_redo());
	assert_int_equal(UN_ERR_SUCCESS, un_group_redo());
	assert_int_equal(UN_ERR_NONE, un_group_redo());
}

TEST(disbalance)
{
	static int undo_levels = 10;

	init_undo_list_for_tests(&execute_fail, &undo_levels);
	un_reset();

	un_group_open("msg0");
	assert_int_equal(0, un_group_add_op(OP_COPY, NULL, NULL, "do_msg0",
			"undo_msg0"));
	un_group_close();

	un_group_open("msg1");
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg1",
			"undo_msg1"));
	un_group_close();

	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	assert_int_equal(UN_ERR_BROKEN, un_group_undo());
	assert_int_equal(UN_ERR_NONE, un_group_undo());
	assert_int_equal(UN_ERR_BALANCE, un_group_redo());
	assert_int_equal(UN_ERR_SUCCESS, un_group_redo());
	assert_int_equal(UN_ERR_NONE, un_group_redo());
}

TEST(cannot_be_undone)
{
	un_group_open("msg0");
	assert_int_equal(0, un_group_add_op(OP_REMOVE, NULL, NULL, "do_msg0", ""));
	un_group_close();

	assert_int_equal(UN_ERR_NOUNDO, un_group_undo());
}

TEST(removing_of_incomplete_groups)
{
	static int undo_levels = 10;
	int i;

	init_undo_list_for_tests(&exec_dummy, &undo_levels);

	un_group_open("msg0");
	for(i = 0; i < 10; i++)
		assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg0",
				"undo_msg0"));
	un_group_close();

	un_group_open("msg1");
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg1",
			"undo_msg1"));
	un_group_close();

	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	assert_int_equal(UN_ERR_NONE, un_group_undo());
}

static OpsResult
exec_skip(OPS op, void *data, const char *src, const char *dst)
{
	return OPS_SKIPPED;
}

TEST(skipping)
{
	static int undo_levels = 10;
	init_undo_list_for_tests(&exec_skip, &undo_levels);

	un_group_open("msg0");
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg0",
			"undo_msg0"));
	un_group_close();

	assert_int_equal(UN_ERR_SKIPPED, un_group_undo());
	assert_int_equal(UN_ERR_BALANCE, un_group_redo());
}

TEST(to_many_commands_and_continue)
{
	static int undo_levels = 3;
	init_undo_list_for_tests(&exec_skip, &undo_levels);

	un_group_open("msg0");
	un_group_close();

	un_group_reopen_last();
	free(un_replace_group_msg("HI"));
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg0",
			"undo_msg0"));
	un_group_close();
	un_group_reopen_last();
	free(un_replace_group_msg("HI"));
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg0",
			"undo_msg0"));
	un_group_close();
	un_group_reopen_last();
	free(un_replace_group_msg("HI"));
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg0",
			"undo_msg0"));
	un_group_close();
	un_group_reopen_last();
	free(un_replace_group_msg("HI"));
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg0",
			"undo_msg0"));
	un_group_close();
	un_group_reopen_last();
	free(un_replace_group_msg("HI"));
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg0",
			"undo_msg0"));
	un_group_close();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
