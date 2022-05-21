#include <stic.h>

#include <stdlib.h>

#include "../../src/undo.h"

#include "test.h"

static OpsResult
execute_dummy(OPS op, void *data, const char src[], const char dst[])
{
	return OPS_SUCCEEDED;
}

TEST(undolevel_change_to_smaller)
{
	static int undo_levels = 4;

	init_undo_list_for_tests(&execute_dummy, &undo_levels);

	un_group_open("msg4");
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg4",
			"undo_msg4"));
	un_group_close();

	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	assert_int_equal(UN_ERR_NONE, un_group_undo());
}

TEST(zero_undolevel)
{
	static int undo_levels = 0;

	init_undo_list_for_tests(&execute_dummy, &undo_levels);

	un_group_open("msg4");
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg4",
			"undo_msg4"));
	un_group_close();

	assert_int_equal(UN_ERR_NONE, un_group_undo());
}

TEST(negative_undolevel)
{
	static int undo_levels = -1;

	init_undo_list_for_tests(&execute_dummy, &undo_levels);

	un_group_open("msg4");
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg4",
			"undo_msg4"));
	un_group_close();

	assert_int_equal(UN_ERR_NONE, un_group_undo());
}

TEST(too_many_commands)
{
	un_group_open("msg4");
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg40",
				"undo_msg40"));
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg41",
				"undo_msg41"));
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg42",
				"undo_msg42"));
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg43",
				"undo_msg43"));
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg44",
				"undo_msg44"));
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg45",
				"undo_msg45"));
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg46",
				"undo_msg46"));
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg47",
				"undo_msg47"));
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg48",
				"undo_msg48"));
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg49",
				"undo_msg49"));
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg4a",
				"undo_msg4a"));
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg4b",
				"undo_msg4b"));
	un_group_close();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
