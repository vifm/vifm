#include "seatest.h"

#include "../../src/engine/cmds.h"
#include "../../src/commands.h"

static int builtin_cmd(const cmd_info_t* cmd_info);

static const cmd_add_t commands[] =
{
	{ .name = "builtin", .abbr = NULL, .handler = builtin_cmd, .id = -1,    .range = 0,    .cust_sep = 0,
		.emark = 1,        .qmark = 0,   .expand = 0,            .regexp = 0, .min_args = 0, .max_args = 0, .bg = 1, },
};

static int called;
static int bg;

static int
builtin_cmd(const cmd_info_t* cmd_info)
{
	called = 1;
	bg = cmd_info->bg;
	return 0;
}

static void
setup(void)
{
	lwin.selected_files = 0;
	lwin.list_rows = 0;

	curr_view = &lwin;

	init_commands();

	add_builtin_commands(commands, ARRAY_LEN(commands));
}

static void
test_space_amp(void)
{
	called = 0;
	assert_int_equal(0, exec_commands("builtin &", &lwin, GET_COMMAND));
	assert_int_equal(1, called);
	assert_int_equal(1, bg);
}

static void
test_space_bg_bar(void)
{
	called = 0;
	assert_int_equal(0, exec_commands("builtin &|", &lwin, GET_COMMAND));
	assert_int_equal(1, called);
	assert_int_equal(1, bg);
}

static void
test_bg_space_bar(void)
{
	called = 0;
	assert_int_equal(0, exec_commands("builtin& |", &lwin, GET_COMMAND));
	assert_int_equal(1, called);
	assert_int_equal(1, bg);
}

static void
test_space_bg_space_bar(void)
{
	called = 0;
	assert_int_equal(0, exec_commands("builtin & |", &lwin, GET_COMMAND));
	assert_int_equal(1, called);
	assert_int_equal(1, bg);
}

void
commands_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(reset_cmds);

	run_test(test_space_amp);
	run_test(test_space_bg_bar);
	run_test(test_bg_space_bar);
	run_test(test_space_bg_space_bar);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
