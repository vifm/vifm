#include <stdlib.h>
#include <string.h>

#include "seatest.h"

#include "../../src/engine/cmds.h"
#include "../../src/utils/macros.h"

extern cmds_conf_t cmds_conf;

static cmd_info_t cmdi;
static char *arg;

static int goto_cmd(const cmd_info_t* cmd_info);
static int exec_cmd(const cmd_info_t* cmd_info);
static int call_cmd(const cmd_info_t* cmd_info);
static int delete_cmd(const cmd_info_t* cmd_info);
static int edia_cmd(const cmd_info_t* cmd_info);
static int edit_cmd(const cmd_info_t* cmd_info);
static int file_cmd(const cmd_info_t* cmd_info);
static int filter_cmd(const cmd_info_t* cmd_info);
static int history_cmd(const cmd_info_t* cmd_info);
static int invert_cmd(const cmd_info_t* cmd_info);
static int substitute_cmd(const cmd_info_t* cmd_info);
static int quit_cmd(const cmd_info_t* cmd_info);

static const cmd_add_t commands[] =
{
	{ .name = "",           .abbr = NULL,  .handler = goto_cmd,       .id = -1,    .range = 1,    .cust_sep = 0,
		.emark = 0,           .qmark = 0,    .expand = 0,               .regexp = 0, .min_args = 0, .max_args = 0,       .bg = 0,     },
	{ .name = "!",          .abbr = NULL,  .handler = exec_cmd,       .id = -1,    .range = 0,    .cust_sep = 0,
		.emark = 1,           .qmark = 1,    .expand = 0,               .regexp = 0, .min_args = 1, .max_args = 1,       .bg = 1,     },
	{ .name = "call",       .abbr = "cal", .handler = call_cmd,       .id = -1,    .range = 0,    .cust_sep = 0,       .quote = 1,
		.emark = 0,           .qmark = 2,    .expand = 1,               .regexp = 0, .min_args = 1, .max_args = 1,       .bg = 0,     },
	{ .name = "delete",     .abbr = "d",   .handler = delete_cmd,     .id =  1,    .range = 1,    .cust_sep = 0,       .quote = 1,
		.emark = 1,           .qmark = 0,    .expand = 0,               .regexp = 0, .min_args = 0, .max_args = 1,       .bg = 0,     },
	{ .name = "edia",       .abbr = NULL,  .handler = edia_cmd,       .id = -1,    .range = 1,    .cust_sep = 0,
		.emark = 0,           .qmark = 0,    .expand = 0,               .regexp = 0, .min_args = 0, .max_args = NOT_DEF, .bg = 0,     },
	{ .name = "edit",       .abbr = "e",   .handler = edit_cmd,       .id = -1,    .range = 1,    .cust_sep = 0,
		.emark = 0,           .qmark = 0,    .expand = 0,               .regexp = 0, .min_args = 0, .max_args = NOT_DEF, .bg = 0,     },
	{ .name = "file",       .abbr = NULL,  .handler = file_cmd,       .id = -1,    .range = 1,    .cust_sep = 0,
		.emark = 0,           .qmark = 1,    .expand = 0,               .regexp = 0, .min_args = 0, .max_args = 0,       .bg = 1,     },
	{ .name = "filter",     .abbr = "fil", .handler = filter_cmd,     .id = -1,    .range = 1,    .cust_sep = 0,
		.emark = 1,           .qmark = 1,    .expand = 0,               .regexp = 1, .min_args = 0, .max_args = NOT_DEF, .bg = 0,     },
	{ .name = "history",    .abbr = "his", .handler = history_cmd,    .id = -1,    .range = 0,    .cust_sep = 0,
		.emark = 0,           .qmark = 0,    .expand = 0,               .regexp = 0, .min_args = 0, .max_args = 0,       .bg = 0,     },
	{ .name = "invert",     .abbr = NULL,  .handler = invert_cmd,     .id = -1,    .range = 0,    .cust_sep = 0,
		.emark = 0,           .qmark = 1,    .expand = 0,               .regexp = 0, .min_args = 0, .max_args = 0,       .bg = 0,     },
	{ .name = "substitute", .abbr = "s",   .handler = substitute_cmd, .id = -1,    .range = 0,    .cust_sep = 1,
		.emark = 1,           .qmark = 0,    .expand = 0,               .regexp = 0, .min_args = 0, .max_args = 3,       .bg = 0,     },
	{ .name = "quit",       .abbr = "q",   .handler = quit_cmd,       .id = -1,    .range = 0,    .cust_sep = 0,
		.emark = 1,           .qmark = 0,    .expand = 0,               .regexp = 0, .min_args = 0, .max_args = 0,       .bg = 0,     },
};

static int
goto_cmd(const cmd_info_t* cmd_info)
{
	cmdi = *cmd_info;
	return 0;
}

static int
exec_cmd(const cmd_info_t* cmd_info)
{
	cmdi = *cmd_info;
	if(cmdi.argc > 0)
	{
		free(arg);
		arg = strdup(cmdi.argv[0]);
	}
	return 0;
}

static int
call_cmd(const cmd_info_t* cmd_info)
{
	cmdi = *cmd_info;
	if(cmdi.argc > 0)
	{
		free(arg);
		arg = strdup(cmdi.args);
	}
	return 0;
}

static int
delete_cmd(const cmd_info_t* cmd_info)
{
	cmdi = *cmd_info;
	if(cmdi.argc > 0)
	{
		free(arg);
		arg = strdup(cmdi.argv[0]);
	}
	return 0;
}

static int
edia_cmd(const cmd_info_t* cmd_info)
{
	cmdi.usr1 = 1;
	return 0;
}

static int
edit_cmd(const cmd_info_t* cmd_info)
{
	cmdi = *cmd_info;
	cmdi.usr1 = 2;
	if(cmdi.argc > 0)
	{
		free(arg);
		arg = strdup(cmdi.argv[0]);
	}
	return 0;
}

static int
file_cmd(const cmd_info_t* cmd_info)
{
	cmdi = *cmd_info;
	return 0;
}

static int
filter_cmd(const cmd_info_t* cmd_info)
{
	cmdi = *cmd_info;
	if(cmdi.argc > 0)
	{
		free(arg);
		arg = strdup(cmdi.argv[0]);
	}
	return 0;
}

static int
history_cmd(const cmd_info_t* cmd_info)
{
	cmdi = *cmd_info;
	return 0;
}

static int
invert_cmd(const cmd_info_t* cmd_info)
{
	cmdi = *cmd_info;
	return 0;
}

static int
substitute_cmd(const cmd_info_t* cmd_info)
{
	cmdi = *cmd_info;
	if(cmdi.argc > 0)
	{
		free(arg);
		arg = strdup(cmdi.argv[0]);
	}
	return 0;
}

static int
quit_cmd(const cmd_info_t* cmd_info)
{
	cmdi = *cmd_info;
	return 0;
}

static void
setup(void)
{
	add_builtin_commands(commands, ARRAY_LEN(commands));
}

static void
teardown(void)
{
}

static void
test_trimming(void)
{
	assert_int_equal(0, execute_cmd("q"));
	assert_int_equal(0, execute_cmd(" q"));
}

static void
test_range_acceptance(void)
{
	assert_int_equal(0, execute_cmd("%delete"));
	assert_int_equal(CMDS_ERR_NO_RANGE_ALLOWED, execute_cmd("%history"));
}

static void
test_single_quote_doubling(void)
{
	assert_int_equal(0, execute_cmd("delete 'file''with''single''quotes'"));
	assert_string_equal("file'with'single'quotes", arg);
}

static void
test_range(void)
{
	cmds_conf.begin = 0;
	cmds_conf.current = 50;
	cmds_conf.end = 100;

	assert_int_equal(0, execute_cmd("%delete!"));
	assert_int_equal(0, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd("$delete!"));
	assert_int_equal(100, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd(".,$delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd(",$delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd("$,.delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd("$,delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd(",delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(50, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd("5,6,7delete!"));
	assert_int_equal(5, cmdi.begin);
	assert_int_equal(6, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd("1,3,7"));
	assert_int_equal(2, cmdi.begin);
	assert_int_equal(6, cmdi.end);

	assert_int_equal(0, execute_cmd("7,3,1"));
	assert_int_equal(2, cmdi.begin);
	assert_int_equal(0, cmdi.end);
}

static void
test_range_plus_minus(void)
{
	cmds_conf.begin = 0;
	cmds_conf.current = 50;
	cmds_conf.end = 100;

	assert_int_equal(0, execute_cmd(".+"));
	assert_int_equal(51, cmdi.begin);
	assert_int_equal(51, cmdi.end);

 	assert_int_equal(0, execute_cmd("+2"));
	assert_int_equal(52, cmdi.begin);
	assert_int_equal(52, cmdi.end);
 
	assert_int_equal(0, execute_cmd(".++"));
	assert_int_equal(52, cmdi.begin);
	assert_int_equal(52, cmdi.end);

	assert_int_equal(0, execute_cmd(".+0-0"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(50, cmdi.end);

	assert_int_equal(0, execute_cmd(".+-"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(50, cmdi.end);

	assert_int_equal(0, execute_cmd(".+5-2"));
	assert_int_equal(53, cmdi.begin);
	assert_int_equal(53, cmdi.end);

	assert_int_equal(0, execute_cmd(".,.+500"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);

	assert_int_equal(0, execute_cmd(".,.-500"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(0, cmdi.end);
}

static void
test_range_and_spaces(void)
{
	cmds_conf.begin = 10;
	cmds_conf.current = 50;
	cmds_conf.end = 100;

	assert_int_equal(0, execute_cmd("% delete!"));
	assert_int_equal(10, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd("  ,  ,  ,   $  ,   .   delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);
}

static void
test_bang_acceptance(void)
{
	assert_int_equal(0, execute_cmd("q"));
	assert_int_equal(CMDS_ERR_NO_BANG_ALLOWED, execute_cmd("history!"));
}

static void
test_bang(void)
{
	assert_int_equal(0, execute_cmd("q"));
	assert_false(cmdi.emark);

	assert_int_equal(0, execute_cmd("q!"));
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd("!vim"));
	assert_false(cmdi.emark);

	assert_int_equal(0, execute_cmd("!!vim"));
	assert_true(cmdi.emark);

	assert_int_equal(CMDS_ERR_TRAILING_CHARS, execute_cmd("q !"));
}

static void
test_qmark_acceptance(void)
{
	assert_int_equal(0, execute_cmd("invert?"));
	assert_int_equal(CMDS_ERR_NO_QMARK_ALLOWED, execute_cmd("history?"));
}

static void
test_qmark(void)
{
	assert_int_equal(0, execute_cmd("invert"));
	assert_false(cmdi.qmark);

	assert_int_equal(0, execute_cmd("invert?"));
	assert_true(cmdi.qmark);

	assert_int_equal(CMDS_ERR_TRAILING_CHARS, execute_cmd("invert ?"));
}

static void
test_args(void)
{
	assert_int_equal(CMDS_ERR_TOO_FEW_ARGS, execute_cmd("call"));
	assert_int_equal(CMDS_ERR_TRAILING_CHARS, execute_cmd("call a b"));

	assert_int_equal(0, execute_cmd("call a"));
	assert_int_equal(1, cmdi.argc);

	assert_int_equal(0, execute_cmd("delete"));
	assert_int_equal(0, cmdi.argc);

	assert_int_equal(0, execute_cmd("edit a b c d"));
	assert_int_equal(4, cmdi.argc);
}

static void
test_count(void)
{
	assert_int_equal(0, execute_cmd("delete 10"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("10", arg);

	assert_int_equal(0, execute_cmd("delete10"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("10", arg);
}

static void
test_end_characters(void)
{
	assert_int_equal(0, execute_cmd("call a"));
	assert_int_equal(CMDS_ERR_TRAILING_CHARS, execute_cmd("call, a"));
}

static void
test_custom_separator(void)
{
	assert_int_equal(0, execute_cmd("s"));
	assert_int_equal(0, cmdi.argc);
	assert_int_equal(0, execute_cmd("s/some"));
	assert_int_equal(1, cmdi.argc);
	assert_int_equal(0, execute_cmd("s/some/thing"));
	assert_int_equal(2, cmdi.argc);
	assert_int_equal(0, execute_cmd("s/some/thing/g"));
	assert_int_equal(3, cmdi.argc);
	assert_int_equal(CMDS_ERR_TRAILING_CHARS, execute_cmd("s/some/thing/g/j"));
}

static void
test_custom_separator_and_arg_format(void)
{
	assert_int_equal(0, execute_cmd("s/\"so\"me\"/thing/g"));
	assert_int_equal(3, cmdi.argc);
	assert_string_equal("\"so\"me\"", arg);

	assert_int_equal(0, execute_cmd("s/'some'/thing/g"));
	assert_int_equal(3, cmdi.argc);
	assert_string_equal("'some'", arg);
}

static void
test_custom_separator_and_emark(void)
{
	assert_int_equal(0, execute_cmd("s!"));
	assert_int_equal(0, cmdi.argc);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd("s!/some"));
	assert_int_equal(1, cmdi.argc);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd("s!/some/thing"));
	assert_int_equal(2, cmdi.argc);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd("s!/some/thing/g"));
	assert_int_equal(3, cmdi.argc);
	assert_true(cmdi.emark);

	assert_int_equal(CMDS_ERR_TRAILING_CHARS, execute_cmd("s!/some/thing/g/j"));
}

static void
test_regexp_flag(void)
{
	assert_int_equal(0, execute_cmd("e /te|xt/"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("/te|xt/", arg);

	assert_int_equal(0, execute_cmd("filter /te|xt/"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("te|xt", arg);
}

static void
test_backgrounding(void)
{
	assert_int_equal(0, execute_cmd("e &"));
	assert_int_equal(1, cmdi.argc);
	assert_int_equal(0, cmdi.bg);

	assert_int_equal(0, execute_cmd("!vim&"));
	assert_int_equal(1, cmdi.argc);
	assert_int_equal(0, cmdi.bg);

	assert_int_equal(0, execute_cmd("!vim &"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("vim", arg);
	assert_int_equal(1, cmdi.bg);
}

static void
test_no_args_after_qmark_1(void)
{
	assert_int_equal(0, execute_cmd("filter?"));
	assert_int_equal(0, execute_cmd("filter?      "));
	assert_int_equal(CMDS_ERR_TRAILING_CHARS, execute_cmd("filter? some_thing"));
}

static void
test_args_after_qmark_2(void)
{
	assert_int_equal(0, execute_cmd("call? arg"));
	assert_int_equal(0, execute_cmd("call? some_thing"));
}

static void
test_no_space_before_e_and_q_marks(void)
{
	assert_int_equal(0, execute_cmd("filter?"));
	assert_int_equal(cmdi.argc, 0);
	assert_true(cmdi.qmark);

	assert_int_equal(0, execute_cmd("filter ?"));
	assert_int_equal(cmdi.argc, 1);
	assert_false(cmdi.qmark);
}

static void
test_only_one_mark(void)
{
	assert_int_equal(0, execute_cmd("filter?"));
	assert_int_equal(cmdi.argc, 0);
	assert_true(cmdi.qmark);

	assert_int_equal(0, execute_cmd("filter!"));
	assert_int_equal(cmdi.argc, 0);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd("filter!!"));
	assert_int_equal(cmdi.argc, 1);
	assert_true(cmdi.emark);

	assert_int_equal(CMDS_ERR_TRAILING_CHARS, execute_cmd("filter?!"));
	assert_int_equal(CMDS_ERR_TRAILING_CHARS, execute_cmd("filter??"));

	assert_int_equal(0, execute_cmd("filter!?"));
	assert_int_equal(cmdi.argc, 1);
	assert_true(cmdi.emark);
	assert_false(cmdi.qmark);
}

static void
test_args_trimming(void)
{
	assert_int_equal(0, execute_cmd("call hi"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("hi", arg);

	assert_int_equal(0, execute_cmd("call 'hi'"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("'hi'", arg);

	assert_int_equal(0, execute_cmd("call hi "));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("hi", arg);

	assert_int_equal(0, execute_cmd("call hi "));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("hi", arg);

	assert_int_equal(0, execute_cmd("call hi\\  "));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("hi\\ ", arg);
}

static void
test_bg_and_no_args(void)
{
	assert_int_equal(0, execute_cmd("file"));
	assert_false(cmdi.bg);

	assert_int_equal(0, execute_cmd("file &"));
	assert_true(cmdi.bg);
}

static void
test_short_forms(void)
{
	assert_int_equal(0, execute_cmd("e"));
	assert_int_equal(2, cmdi.usr1);

	assert_int_equal(0, execute_cmd("ed"));
	assert_int_equal(2, cmdi.usr1);

	assert_int_equal(0, execute_cmd("edi"));
	assert_int_equal(2, cmdi.usr1);

	assert_int_equal(0, execute_cmd("edit"));
	assert_int_equal(2, cmdi.usr1);

	assert_int_equal(0, execute_cmd("edia"));
	assert_int_equal(1, cmdi.usr1);
}

static void
test_qmark_and_bg(void)
{
	assert_int_equal(0, execute_cmd("file &"));
	assert_true(cmdi.bg);

	assert_int_equal(0, execute_cmd("file?"));
	assert_true(cmdi.qmark);

	assert_int_equal(0, execute_cmd("file? &"));
	assert_true(cmdi.bg);
	assert_true(cmdi.qmark);
}

static void
test_extra_long_command_name(void)
{
	char cmd_name[1024];

	memset(cmd_name, 'a', sizeof(cmd_name) - 1);
	cmd_name[sizeof(cmd_name) - 1] = '\0';

	assert_false(execute_cmd(cmd_name) == 0);
}

static void
test_emark_is_checked_before_number_of_args(void)
{
	assert_int_equal(CMDS_ERR_NO_BANG_ALLOWED, execute_cmd("call!"));
}

static void
test_qmark_is_checked_before_number_of_args(void)
{
	assert_int_equal(CMDS_ERR_NO_QMARK_ALLOWED, execute_cmd("quit? from here"));
}

static void
test_missing_quotes_are_allowed(void)
{
	assert_int_equal(CMDS_ERR_INVALID_ARG, execute_cmd("call 'ismissing"));
}

void
input_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_trimming);
	run_test(test_range_acceptance);
	run_test(test_single_quote_doubling);
	run_test(test_range);
	run_test(test_range_plus_minus);
	run_test(test_range_and_spaces);
	run_test(test_bang_acceptance);
	run_test(test_bang);
	run_test(test_qmark_acceptance);
	run_test(test_qmark);
	run_test(test_args);
	run_test(test_count);
	run_test(test_end_characters);
	run_test(test_custom_separator);
	run_test(test_custom_separator_and_arg_format);
	run_test(test_custom_separator_and_emark);
	run_test(test_regexp_flag);
	run_test(test_backgrounding);
	run_test(test_no_args_after_qmark_1);
	run_test(test_args_after_qmark_2);
	run_test(test_no_space_before_e_and_q_marks);
	run_test(test_only_one_mark);
	run_test(test_args_trimming);
	run_test(test_bg_and_no_args);
	run_test(test_short_forms);
	run_test(test_qmark_and_bg);
	run_test(test_extra_long_command_name);
	run_test(test_emark_is_checked_before_number_of_args);
	run_test(test_qmark_is_checked_before_number_of_args);
	run_test(test_missing_quotes_are_allowed);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
