#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/undo.h"

static int exec_func(OPS op, void *data, const char *src, const char *dst);
static void init_undo_list_for_tests(un_perform_func exec_func,
		const int *max_levels);
static int op_avail(OPS op);

DEFINE_SUITE();

SETUP()
{
	static int undo_levels = 10;

	init_undo_list_for_tests(&exec_func, &undo_levels);

	cfg.use_system_calls = 1;
	cfg.slow_fs_list = strdup("");
	cfg.delete_prg = strdup("");

	lwin.list_rows = 0;
	lwin.dir_entry = NULL;
	rwin.list_rows = 0;
	rwin.dir_entry = NULL;
}

TEARDOWN()
{
	free(cfg.slow_fs_list);
	free(cfg.delete_prg);

	un_reset();
}

static int
exec_func(OPS op, void *data, const char *src, const char *dst)
{
	return 0;
}

static void
init_undo_list_for_tests(un_perform_func exec_func, const int *max_levels)
{
	un_init(exec_func, &op_avail, NULL, max_levels);
}

static int
op_avail(OPS op)
{
	return op == OP_MOVE;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
