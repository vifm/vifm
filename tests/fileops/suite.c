#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../../src/cfg/config.h"
#include "../../src/ui/color_manager.h"
#include "../../src/ui/ui.h"
#include "../../src/undo.h"

static int exec_func(OPS op, void *data, const char *src, const char *dst);
static void init_undo_list_for_tests(un_perform_func exec_func,
		const int *max_levels);
static int op_avail(OPS op);
static int init_pair_stub(short pair, short f, short b);
static int pair_content_stub(short pair, short *f, short *b);
static int pair_in_use_stub(short int pair);
static void move_pair_stub(short int from, short int to);

DEFINE_SUITE();

SETUP_ONCE()
{
	const colmgr_conf_t colmgr_conf = {
		.max_color_pairs = 256,
		.max_colors = 16,
		.init_pair = &init_pair_stub,
		.pair_content = &pair_content_stub,
		.pair_in_use = &pair_in_use_stub,
		.move_pair = &move_pair_stub,
	};
	colmgr_init(&colmgr_conf);

	cfg.sizefmt.ieci_prefixes = 0;
	cfg.sizefmt.base = 1024;
	cfg.sizefmt.precision = 0;
}

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

static int
init_pair_stub(short pair, short f, short b)
{
	return 0;
}

static int
pair_content_stub(short pair, short *f, short *b)
{
	*f = 0;
	*b = 0;
	return 0;
}

static int
pair_in_use_stub(short int pair)
{
	return 0;
}

static void
move_pair_stub(short int from, short int to)
{
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
