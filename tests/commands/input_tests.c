#include <stic.h>

#include <stdlib.h>
#include <string.h>

#include "../../src/engine/cmds.h"
#include "../../src/utils/macros.h"

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

extern cmds_conf_t cmds_conf;

static cmd_info_t cmdi;
static char *arg;

static const cmd_add_t commands[] = {
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

SETUP()
{
	add_builtin_commands(commands, ARRAY_LEN(commands));
}

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

TEST(leading_whitespace_trimming)
{
	assert_int_equal(0, execute_cmd("q"));
	assert_int_equal(0, execute_cmd(" q"));
	assert_int_equal(0, execute_cmd("   q"));
	assert_int_equal(0, execute_cmd("\tq"));
	assert_int_equal(0, execute_cmd("\t\tq"));
	assert_int_equal(0, execute_cmd("\t\t q"));
	assert_int_equal(0, execute_cmd(" \t \tq"));
	assert_int_equal(0, execute_cmd("\t \tq"));
}

TEST(range_acceptance)
{
	assert_int_equal(0, execute_cmd("%delete"));
	assert_int_equal(0, execute_cmd(",%delete"));
	assert_int_equal(0, execute_cmd(";%delete"));
	assert_int_equal(CMDS_ERR_NO_RANGE_ALLOWED, execute_cmd("%history"));
	assert_int_equal(CMDS_ERR_NO_RANGE_ALLOWED, execute_cmd("%,history"));
	assert_int_equal(CMDS_ERR_NO_RANGE_ALLOWED, execute_cmd("%;history"));
}

TEST(single_quote_doubling)
{
	assert_int_equal(0, execute_cmd("delete 'file''with''single''quotes'"));
	assert_string_equal("file'with'single'quotes", arg);
}

TEST(range)
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

TEST(semicolon_range)
{
	cmds_conf.begin = 0;
	cmds_conf.current = 50;
	cmds_conf.end = 100;

	assert_int_equal(0, execute_cmd(".;$delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd(";$delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd("$;delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd(";;delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(50, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd("5;6;7delete!"));
	assert_int_equal(5, cmdi.begin);
	assert_int_equal(6, cmdi.end);
	assert_true(cmdi.emark);
}

TEST(range_plus_minus)
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

TEST(range_comma_vs_semicolon_bases)
{
	cmds_conf.begin = 0;
	cmds_conf.current = 0;
	cmds_conf.end = 3;

	assert_int_equal(0, execute_cmd("2,+2"));
	assert_int_equal(1, cmdi.begin);
	assert_int_equal(2, cmdi.end);

	assert_int_equal(0, execute_cmd("2;+2"));
	assert_int_equal(1, cmdi.begin);
	assert_int_equal(3, cmdi.end);
}

TEST(range_and_spaces)
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

TEST(empty_range_empty_command_called)
{
	cmds_conf.begin = 10;
	cmds_conf.current = 50;
	cmds_conf.end = 100;

	assert_int_equal(0, execute_cmd(""));
	assert_int_equal(NOT_DEF, cmdi.begin);
	assert_int_equal(NOT_DEF, cmdi.end);

	assert_int_equal(0, execute_cmd(","));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(50, cmdi.end);

	assert_int_equal(0, execute_cmd(";"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(50, cmdi.end);
}

TEST(bang_acceptance)
{
	assert_int_equal(0, execute_cmd("q"));
	assert_int_equal(CMDS_ERR_NO_BANG_ALLOWED, execute_cmd("history!"));
}

TEST(bang)
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

TEST(qmark_acceptance)
{
	assert_int_equal(0, execute_cmd("invert?"));
	assert_int_equal(CMDS_ERR_NO_QMARK_ALLOWED, execute_cmd("history?"));
}

TEST(qmark)
{
	assert_int_equal(0, execute_cmd("invert"));
	assert_false(cmdi.qmark);

	assert_int_equal(0, execute_cmd("invert?"));
	assert_true(cmdi.qmark);

	assert_int_equal(CMDS_ERR_TRAILING_CHARS, execute_cmd("invert ?"));
}

TEST(args)
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

TEST(count)
{
	assert_int_equal(0, execute_cmd("delete 10"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("10", arg);

	assert_int_equal(0, execute_cmd("delete10"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("10", arg);
}

TEST(end_characters)
{
	assert_int_equal(0, execute_cmd("call a"));
	assert_int_equal(CMDS_ERR_TRAILING_CHARS, execute_cmd("call, a"));
}

TEST(custom_separator)
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

TEST(custom_separator_and_arg_format)
{
	assert_int_equal(0, execute_cmd("s/\"so\"me\"/thing/g"));
	assert_int_equal(3, cmdi.argc);
	assert_string_equal("\"so\"me\"", arg);

	assert_int_equal(0, execute_cmd("s/'some'/thing/g"));
	assert_int_equal(3, cmdi.argc);
	assert_string_equal("'some'", arg);
}

TEST(custom_separator_and_emark)
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

TEST(regexp_flag_strips_slashes)
{
	assert_int_equal(0, execute_cmd("e /te|xt/"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("/te|xt/", arg);

	assert_int_equal(0, execute_cmd("filter /te|xt/"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("te|xt", arg);
}

TEST(regexp_flag_slashes_and_spaces)
{
	assert_int_equal(0, execute_cmd("filter /te|xt/i"));
	assert_int_equal(2, cmdi.argc);
	assert_string_equal("te|xt", arg);

	assert_int_equal(0, execute_cmd("filter/te|xt/i"));
	assert_int_equal(2, cmdi.argc);
	assert_string_equal("te|xt", arg);
}

TEST(regexp_flag_bang_slashes_and_spaces)
{
	assert_int_equal(0, execute_cmd("filter! /te|xt/i"));
	assert_int_equal(2, cmdi.argc);
	assert_string_equal("te|xt", arg);

	assert_int_equal(0, execute_cmd("filter!/te|xt/i"));
	assert_int_equal(2, cmdi.argc);
	assert_string_equal("te|xt", arg);
}

TEST(backgrounding)
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

TEST(no_args_after_qmark_1)
{
	assert_int_equal(0, execute_cmd("filter?"));
	assert_int_equal(0, execute_cmd("filter?      "));
	assert_int_equal(CMDS_ERR_TRAILING_CHARS, execute_cmd("filter? some_thing"));
}

TEST(args_after_qmark_2)
{
	assert_int_equal(0, execute_cmd("call? arg"));
	assert_int_equal(0, execute_cmd("call? some_thing"));
}

TEST(no_space_before_e_and_q_marks)
{
	assert_int_equal(0, execute_cmd("filter?"));
	assert_int_equal(cmdi.argc, 0);
	assert_true(cmdi.qmark);

	assert_int_equal(0, execute_cmd("filter ?"));
	assert_int_equal(cmdi.argc, 1);
	assert_false(cmdi.qmark);
}

TEST(only_one_mark)
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

TEST(args_whitespace_trimming)
{
	assert_int_equal(0, execute_cmd("call hi"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("hi", arg);

	assert_int_equal(0, execute_cmd("call\t'hi'"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("'hi'", arg);

	assert_int_equal(0, execute_cmd("call\t hi "));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("hi", arg);

	assert_int_equal(0, execute_cmd("call hi \t"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("hi", arg);

	assert_int_equal(0, execute_cmd("call\t \thi\\  "));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("hi\\ ", arg);
}

TEST(bg_and_no_args)
{
	assert_int_equal(0, execute_cmd("file"));
	assert_false(cmdi.bg);

	assert_int_equal(0, execute_cmd("file &"));
	assert_true(cmdi.bg);
}

TEST(bg_followed_by_whitespace)
{
	assert_int_equal(0, execute_cmd("file & "));
	assert_true(cmdi.bg);

	assert_int_equal(0, execute_cmd("file &  "));
	assert_true(cmdi.bg);

	assert_int_equal(0, execute_cmd("file& "));
	assert_true(cmdi.bg);

	assert_int_equal(0, execute_cmd("file&  "));
	assert_true(cmdi.bg);
}

TEST(short_forms)
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

TEST(qmark_and_bg)
{
	assert_int_equal(0, execute_cmd("file &"));
	assert_true(cmdi.bg);

	assert_int_equal(0, execute_cmd("file?"));
	assert_true(cmdi.qmark);

	assert_int_equal(0, execute_cmd("file? &"));
	assert_true(cmdi.bg);
	assert_true(cmdi.qmark);
}

TEST(extra_long_command_name)
{
	char cmd_name[1024];

	memset(cmd_name, 'a', sizeof(cmd_name) - 1);
	cmd_name[sizeof(cmd_name) - 1] = '\0';

	assert_false(execute_cmd(cmd_name) == 0);
}

TEST(emark_is_checked_before_number_of_args)
{
	assert_int_equal(CMDS_ERR_NO_BANG_ALLOWED, execute_cmd("call!"));
}

TEST(qmark_is_checked_before_number_of_args)
{
	assert_int_equal(CMDS_ERR_NO_QMARK_ALLOWED, execute_cmd("quit? from here"));
}

TEST(missing_quotes_are_allowed)
{
	assert_int_equal(CMDS_ERR_INVALID_ARG, execute_cmd("call 'ismissing"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
