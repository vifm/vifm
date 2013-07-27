#include "test.h"

#include <assert.h>

#include "seatest.h"

#include "../../src/ops.h"
#include "../../src/undo.h"

void undolist_test(void);
void undo_test(void);
void undolevels_test(void);
void last_cmd_group_empty_tests(void);

static int
exec_func(OPS op, void *data, const char *src, const char *dst)
{
	return 0;
}

static int
op_avail(OPS op)
{
	return op == OP_MOVE;
}

void
init_undo_list_for_tests(perform_func exec_func, const int *max_levels)
{
	init_undo_list(exec_func, &op_avail, max_levels);
}

static void
setup(void)
{
	static int undo_levels = 10;
	int ret_code;

	init_undo_list_for_tests(&exec_func, &undo_levels);

	cmd_group_begin("msg1");
	ret_code = add_operation(OP_MOVE, NULL, NULL, "do_msg1", "undo_msg1");
	assert(ret_code == 0);
	cmd_group_end();

	cmd_group_begin("msg2");
	ret_code = add_operation(OP_MOVE, NULL, NULL, "do_msg2_cmd1",
			"undo_msg2_cmd1");
	assert(ret_code == 0);
	ret_code = add_operation(OP_MOVE, NULL, NULL, "do_msg2_cmd2",
			"undo_msg2_cmd2");
	assert(ret_code == 0);
	cmd_group_end();

	cmd_group_begin("msg3");
	ret_code = add_operation(OP_MOVE, NULL, NULL, "do_msg3", "undo_msg3");
	assert(ret_code == 0);
	cmd_group_end();
}

static void
teardown(void)
{
	reset_undo_list();
}

static void
all_tests(void)
{
	undolist_test();
	undo_test();
	undolevels_test();
	last_cmd_group_empty_tests();
}

int
main(int argc, char **argv)
{
	suite_setup(setup);
	suite_teardown(teardown);

	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
