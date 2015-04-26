#include <stic.h>

#include "../../src/engine/cmds.h"
#include "../../src/commands.h"

static int builtin_cmd(const cmd_info_t* cmd_info);

static const cmd_add_t commands[] = {
	{ .name = "builtin", .abbr = NULL, .handler = builtin_cmd, .id = -1,    .range = 0,    .cust_sep = 0,
		.emark = 1,        .qmark = 0,   .expand = 0,            .regexp = 0, .min_args = 0, .max_args = 0, .bg = 1, },
	{ .name = "onearg",  .abbr = NULL, .handler = builtin_cmd, .id = -1,    .range = 0,    .cust_sep = 0,
		.emark = 0,        .qmark = 0,   .expand = 0,            .regexp = 0, .min_args = 1, .max_args = 1, .bg = 0, },
};

static int called;
static int bg;

SETUP()
{
	lwin.selected_files = 0;
	lwin.list_rows = 0;

	curr_view = &lwin;

	init_commands();

	add_builtin_commands(commands, ARRAY_LEN(commands));
}

TEARDOWN()
{
	reset_cmds();
}

static int
builtin_cmd(const cmd_info_t* cmd_info)
{
	called = 1;
	bg = cmd_info->bg;
	return 0;
}

TEST(space_amp)
{
	called = 0;
	assert_int_equal(0, exec_commands("builtin &", &lwin, CIT_COMMAND));
	assert_int_equal(1, called);
	assert_int_equal(1, bg);
}

TEST(space_bg_bar)
{
	called = 0;
	assert_int_equal(0, exec_commands("builtin &|", &lwin, CIT_COMMAND));
	assert_int_equal(1, called);
	assert_int_equal(1, bg);
}

TEST(bg_space_bar)
{
	called = 0;
	assert_int_equal(0, exec_commands("builtin& |", &lwin, CIT_COMMAND));
	assert_int_equal(1, called);
	assert_int_equal(1, bg);
}

TEST(space_bg_space_bar)
{
	called = 0;
	assert_int_equal(0, exec_commands("builtin & |", &lwin, CIT_COMMAND));
	assert_int_equal(1, called);
	assert_int_equal(1, bg);
}

TEST(non_printable_arg)
{
	called = 0;
	/* \x0C is Ctrl-L. */
	assert_int_equal(0, exec_commands("onearg \x0C", &lwin, CIT_COMMAND));
	assert_true(called);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
