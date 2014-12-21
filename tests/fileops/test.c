#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "seatest.h"

#include "../../src/cfg/config.h"
#include "../../src/undo.h"

void generic_tests(void);

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
	init_undo_list(exec_func, &op_avail, NULL, max_levels);
}

static void
setup(void)
{
	static int undo_levels = 10;

	init_undo_list_for_tests(&exec_func, &undo_levels);

	cfg.use_system_calls = 1;
#ifndef _WIN32
	cfg.slow_fs_list = strdup("");
#endif
}

static void
teardown(void)
{
#ifndef _WIN32
	free(cfg.slow_fs_list);
#endif

	reset_undo_list();
}

static void
all_tests(void)
{
	generic_tests();
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
