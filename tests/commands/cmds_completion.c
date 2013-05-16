#include <stdlib.h>
#include <string.h>

#include "seatest.h"

#include "../../src/engine/cmds.h"
#include "../../src/engine/completion.h"
#include "../../src/utils/macros.h"

extern cmds_conf_t cmds_conf;

static cmd_info_t cmdi;

static int dummy_cmd(const cmd_info_t *cmd_info);
static int delete_cmd(const cmd_info_t *cmd_info);
static int skip_at_beginning(int id, const char *args);

enum { COM_WINDO };

static const cmd_add_t commands[] = {
	{ .name = "",       .abbr = NULL, .handler = dummy_cmd,  .id = -1,    .range = 1,    .cust_sep = 0,
		.emark = 0,       .qmark = 0,   .expand = 0,           .regexp = 0, .min_args = 0, .max_args = 0, },
	{ .name = "delete", .abbr = "d",  .handler = delete_cmd, .id = 1,     .range = 1,    .cust_sep = 0,
		.emark = 1,       .qmark = 0,   .expand = 0,           .regexp = 0, .min_args = 0, .max_args = 1, },
	{ .name = "yank",   .abbr = "y",  .handler = dummy_cmd,  .id = -10,   .range = 0,    .cust_sep = 0,
		.emark = 0,       .qmark = 0,   .expand = 0,           .regexp = 0, .min_args = 0, .max_args = 1, },
	{ .name = "windo",            .abbr = NULL,    .emark = 0,  .id = COM_WINDO,       .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = dummy_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
};

static int
dummy_cmd(const cmd_info_t *cmd_info)
{
	return 0;
}

static int
delete_cmd(const cmd_info_t *cmd_info)
{
	cmdi = *cmd_info;
	return 0;
}

static int
skip_at_beginning(int id, const char *args)
{
	if(id == COM_WINDO)
	{
		return 0;
	}
	return -1;
}

static void
setup(void)
{
	add_builtin_commands(commands, ARRAY_LEN(commands));
	cmds_conf.skip_at_beginning = &skip_at_beginning;
}

static void
teardown(void)
{
}

static void
test_skip_goto(void)
{
	char *completion;

	reset_completion();
	assert_int_equal(0, complete_cmd(""));
	assert_int_equal(7, get_completion_count());
	if(get_completion_count() == 0)
		return;
	completion = next_completion();
	assert_string_equal("comclear", completion);
	free(completion);
	completion = next_completion();
	assert_string_equal("command", completion);
	free(completion);
	completion = next_completion();
	assert_string_equal("delcommand", completion);
	free(completion);
}

static void
test_skip_abbreviations(void)
{
	char *completion;

	reset_completion();
	assert_int_equal(0, complete_cmd("d"));
	assert_int_equal(3, get_completion_count());
	if(get_completion_count() == 0)
		return;
	completion = next_completion();
	assert_string_equal("delcommand", completion);
	free(completion);
	completion = next_completion();
	assert_string_equal("delete", completion);
	free(completion);
}

static void
test_dont_remove_range(void)
{
	char *completion;

	reset_completion();
	assert_int_equal(2, complete_cmd("% del"));
	assert_int_equal(3, get_completion_count());
	if(get_completion_count() == 0)
		return;
	completion = next_completion();
	assert_string_equal("delcommand", completion);
	free(completion);

	reset_completion();
	assert_int_equal(1, complete_cmd("3del"));
	assert_int_equal(3, get_completion_count());
	if(get_completion_count() == 0)
		return;
	completion = next_completion();
	assert_string_equal("delcommand", completion);
	free(completion);
}

static void
test_dont_remove_cmd(void)
{
	char *completion;

	reset_completion();
	assert_int_equal(7, complete_cmd("% dele "));
	assert_int_equal(3, get_completion_count());
	if(get_completion_count() == 0)
		return;
	completion = next_completion();
	assert_string_equal("fastrun", completion);
	free(completion);
	completion = next_completion();
	assert_string_equal("followlinks", completion);
	free(completion);
}

static void
test_skip_complete_args(void)
{
	char *completion;

	reset_completion();
	assert_int_equal(17, complete_cmd("% dele fast slow "));
	assert_int_equal(3, get_completion_count());
	if(get_completion_count() == 0)
		return;
	completion = next_completion();
	assert_string_equal("fastrun", completion);
	free(completion);
	completion = next_completion();
	assert_string_equal("followlinks", completion);
	free(completion);
}

static void
test_com_completion(void)
{
	char *completion;

	assert_int_equal(0, execute_cmd("command udf a"));

	reset_completion();
	assert_int_equal(4, complete_cmd("com u"));
	assert_int_equal(2, get_completion_count());
	if(get_completion_count() == 0)
		return;
	completion = next_completion();
	assert_string_equal("udf", completion);
	free(completion);
}

static void
test_delc_completion(void)
{
	char *completion;

	assert_int_equal(0, execute_cmd("command udf a"));

	reset_completion();
	assert_int_equal(5, complete_cmd("delc u"));
	assert_int_equal(2, get_completion_count());
	if(get_completion_count() == 0)
		return;
	completion = next_completion();
	assert_string_equal("udf", completion);
	free(completion);
}

static void
test_windo_completion(void)
{
	char *completion;

	reset_completion();
	assert_int_equal(0, complete_cmd("win"));
	assert_int_equal(2, get_completion_count());
	if(get_completion_count() == 0)
		return;
	completion = next_completion();
	assert_string_equal("windo", completion);
	free(completion);
}

static void
test_windo_windo_completion(void)
{
	char *completion;

	reset_completion();
	assert_int_equal(12, complete_cmd("windo windo "));
	assert_int_equal(7, get_completion_count());
	if(get_completion_count() == 0)
		return;
	completion = next_completion();
	assert_string_equal("comclear", completion);
	free(completion);
}

static void
test_windo_args_completion(void)
{
	char *completion;

	reset_completion();
	assert_int_equal(6, complete_cmd("windo del"));
	assert_int_equal(3, get_completion_count());
	if(get_completion_count() == 0)
		return;
	completion = next_completion();
	assert_string_equal("delcommand", completion);
	free(completion);
	completion = next_completion();
	assert_string_equal("delete", completion);
	free(completion);
}

static void
test_no_completion_for_negative_ids(void)
{
	reset_completion();
	assert_int_equal(4, complete_cmd("yank "));
	assert_int_equal(0, get_completion_count());
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
	run_test(test_windo_completion);
	run_test(test_windo_windo_completion);
	run_test(test_windo_args_completion);
	run_test(test_no_completion_for_negative_ids);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
