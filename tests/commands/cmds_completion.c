#include <string.h>

#include "seatest.h"

#include "../../src/cmds.h"

#define ARRAY_LEN(x) (sizeof(x)/sizeof((x)[0]))

static struct cmd_info cmdi;

static int goto_cmd(const struct cmd_info* cmd_info);
static int delete_cmd(const struct cmd_info* cmd_info);

static const struct cmd_add commands[] = {
	{ .name = "",       .abbr = NULL, .handler = goto_cmd,   .id = -1,    .range = 1,    .cust_sep = 0,
		.emark = 0,       .qmark = 0,   .expand = 0,           .regexp = 0, .min_args = 0, .max_args = 0, },
	{ .name = "delete", .abbr = "d",  .handler = delete_cmd, .id = 1,     .range = 1,    .cust_sep = 0,
		.emark = 1,       .qmark = 0,   .expand = 0,           .regexp = 0, .min_args = 0, .max_args = 1, },
};

static int
goto_cmd(const struct cmd_info* cmd_info)
{
	return 0;
}

static int
delete_cmd(const struct cmd_info* cmd_info)
{
	cmdi = *cmd_info;
	return 0;
}

static void
setup(void)
{
	add_buildin_commands(commands, ARRAY_LEN(commands));
}

static void
teardown(void)
{
}

static void
test_skip_goto(void)
{
	struct complete_t info;

	assert_int_equal(0, complete_cmd("", &info));
	assert_int_equal(3, info.count);
	if(info.count == 0)
		return;
	assert_string_equal("command", info.buf[0]);
	assert_string_equal("delcommand", info.buf[1]);
	assert_string_equal("delete", info.buf[2]);
}

static void
test_skip_abbreviations(void)
{
	struct complete_t info;

	assert_int_equal(0, complete_cmd("d", &info));
	assert_int_equal(2, info.count);
	if(info.count == 0)
		return;
	assert_string_equal("delcommand", info.buf[0]);
	assert_string_equal("delete", info.buf[1]);
}

static void
test_dont_remove_range(void)
{
	struct complete_t info;

	assert_int_equal(2, complete_cmd("% dele", &info));
	assert_int_equal(1, info.count);
	if(info.count == 0)
		return;
	assert_string_equal("delete", info.buf[0]);

	assert_int_equal(1, complete_cmd("3dele", &info));
	assert_int_equal(1, info.count);
	if(info.count == 0)
		return;
	assert_string_equal("delete", info.buf[0]);
}

static void
test_dont_remove_cmd(void)
{
	struct complete_t info;

	assert_int_equal(7, complete_cmd("% dele ", &info));
	assert_int_equal(2, info.count);
	if(info.count == 0)
		return;
	assert_string_equal("fastrun", info.buf[0]);
	assert_string_equal("followlinks", info.buf[1]);
}

static void
test_skip_complete_args(void)
{
	struct complete_t info;

	assert_int_equal(17, complete_cmd("% dele fast slow ", &info));
	assert_int_equal(2, info.count);
	if(info.count == 0)
		return;
	assert_string_equal("fastrun", info.buf[0]);
	assert_string_equal("followlinks", info.buf[1]);
}

static void
test_com_completion(void)
{
	struct complete_t info;

	assert_int_equal(0, execute_cmd("command udf a"));

	assert_int_equal(4, complete_cmd("com u", &info));
	assert_int_equal(1, info.count);
	if(info.count == 0)
		return;
	assert_string_equal("udf", info.buf[0]);
}

static void
test_delc_completion(void)
{
	struct complete_t info;

	assert_int_equal(0, execute_cmd("command udf a"));

	assert_int_equal(5, complete_cmd("delc u", &info));
	assert_int_equal(1, info.count);
	if(info.count == 0)
		return;
	assert_string_equal("udf", info.buf[0]);
}

void
completion_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_skip_goto);
	run_test(test_skip_abbreviations);
	run_test(test_dont_remove_range);
	run_test(test_dont_remove_cmd);
	run_test(test_skip_complete_args);
	run_test(test_com_completion);
	run_test(test_delc_completion);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
