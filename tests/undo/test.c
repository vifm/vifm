#include <assert.h>

#include "seatest.h"

#include "../../src/undo.h"

void undolist_test(void);
void undo_test(void);
void undolevels_test(void);

static int
exec_func(const char *cmd)
{
	return 0;
}

static void
setup(void)
{
	static int undo_levels = 10;

	init_undo_list(exec_func, &undo_levels);

	cmd_group_begin("msg1");
	assert(add_operation("do_msg1", NULL, NULL, "undo_msg1", NULL, NULL) == 0);
	cmd_group_end();

	cmd_group_begin("msg2");
	assert(add_operation("do_msg2_cmd1", NULL, NULL, "undo_msg2_cmd1", NULL,
			NULL) == 0);
	assert(add_operation("do_msg2_cmd2", NULL, NULL, "undo_msg2_cmd2", NULL,
			NULL) == 0);
	cmd_group_end();

	cmd_group_begin("msg3");
	assert(add_operation("do_msg3", NULL, NULL, "undo_msg3", NULL, NULL) == 0);
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
}

int
main(int argc, char **argv)
{
	suite_setup(setup);
	suite_teardown(teardown);

	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
