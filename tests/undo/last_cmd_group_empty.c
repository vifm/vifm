#include <stic.h>

#include <stdlib.h>

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

	un_reset();
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

TEST(empty_undo_list_true)
{
	assert_true(un_last_group_empty());
}

TEST(empty_group_true)
{
	un_group_open("msg4");
	un_group_close();
	assert_true(un_last_group_empty());
}

TEST(non_empty_group_false)
{
	un_group_open("msg0");
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg0",
			"undo_msg0"));
	un_group_close();
	assert_false(un_last_group_empty());
}

TEST(before_add_in_group_true)
{
	un_group_open("msg0");
	assert_true(un_last_group_empty());
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg0",
			"undo_msg0"));
	un_group_close();
}

TEST(after_add_in_group_false)
{
	un_group_open("msg0");
	assert_int_equal(0, un_group_add_op(OP_MOVE, NULL, NULL, "do_msg0",
			"undo_msg0"));
	assert_false(un_last_group_empty());
	un_group_close();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
