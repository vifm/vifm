#include <stic.h>

#include <stddef.h>

#include "../../src/ops.h"
#include "../../src/undo.h"

#include "test.h"

static OpsResult exec_func(OPS op, void *data, const char src[],
		const char dst[]);
static int op_avail(OPS op);

DEFINE_SUITE();

SETUP()
{
	static int undo_levels = 10;

	init_undo_list_for_tests(&exec_func, &undo_levels);

	un_group_open("msg1");
	assert_success(un_group_add_op(OP_MOVE, NULL, NULL, "do_msg1", "undo_msg1"));
	un_group_close();

	un_group_open("msg2");
	assert_success(un_group_add_op(OP_MOVE, NULL, NULL, "do_msg2_cmd1",
			"undo_msg2_cmd1"));
	assert_success(un_group_add_op(OP_MOVE, NULL, NULL, "do_msg2_cmd2",
			"undo_msg2_cmd2"));
	un_group_close();

	un_group_open("msg3");
	assert_success(un_group_add_op(OP_MOVE, NULL, NULL, "do_msg3", "undo_msg3"));
	un_group_close();
}

TEARDOWN()
{
	un_reset();
}

static OpsResult
exec_func(OPS op, void *data, const char src[], const char dst[])
{
	return OPS_SUCCEEDED;
}

static int
op_avail(OPS op)
{
	return op == OP_MOVE;
}

void
init_undo_list_for_tests(un_perform_func exec_func, const int *max_levels)
{
	un_init(exec_func, &op_avail, NULL, max_levels);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
