#include <stic.h>

#include <stdlib.h>
#include <string.h>

#include "../../src/engine/cmds.h"
#include "../../src/utils/macros.h"

static int dummy_handler(const cmd_info_t* cmd_info);
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
	{ .name = "",                 .abbr = NULL,  .id = -1,      .descr = "descr",
	  .flags = HAS_RANGE,
	  .handler = &goto_cmd,       .min_args = 0, .max_args = 0, },
	{ .name = "!",                .abbr = NULL,  .id = -1,      .descr = "descr",
	  .flags = HAS_EMARK | HAS_QMARK_NO_ARGS | HAS_BG_FLAG,
	  .handler = &exec_cmd,       .min_args = 1, .max_args = 1, },
	{ .name = "call",             .abbr = "cal", .id = -1,      .descr = "descr",
	  .flags = HAS_QUOTED_ARGS | HAS_QMARK_WITH_ARGS | HAS_MACROS_FOR_CMD,
	  .handler = &call_cmd,       .min_args = 1, .max_args = 1, },
	{ .name = "delete",           .abbr = "d",   .id =  1,      .descr = "descr",
	  .flags = HAS_RANGE | HAS_QUOTED_ARGS | HAS_EMARK,
	  .handler = &delete_cmd,     .min_args = 0, .max_args = 1, },
	{ .name = "edia",             .abbr = NULL,  .id = -1,      .descr = "descr",
	  .flags = HAS_RANGE | HAS_RAW_ARGS,
	  .handler = &edia_cmd,       .min_args = 0, .max_args = NOT_DEF, },
	{ .name = "edit",             .abbr = "e",   .id = -1,      .descr = "descr",
	  .flags = HAS_RANGE,
	  .handler = &edit_cmd,       .min_args = 0, .max_args = NOT_DEF, },
	{ .name = "file",             .abbr = NULL,  .id = -1,      .descr = "descr",
	  .flags = HAS_RANGE | HAS_QMARK_NO_ARGS | HAS_BG_FLAG,
	  .handler = &file_cmd,       .min_args = 0, .max_args = 0, },
	{ .name = "filter",           .abbr = "fil", .id = -1,      .descr = "descr",
	  .flags = HAS_RANGE | HAS_EMARK | HAS_QMARK_NO_ARGS | HAS_REGEXP_ARGS,
	  .handler = &filter_cmd,     .min_args = 0, .max_args = NOT_DEF, },
	{ .name = "history",          .abbr = "his", .id = -1,      .descr = "descr",
	  .flags = 0,
	  .handler = &history_cmd,    .min_args = 0, .max_args = 0, },
	{ .name = "invert",           .abbr = NULL,  .id = -1,      .descr = "descr",
	  .flags = HAS_QMARK_NO_ARGS,
	  .handler = &invert_cmd,     .min_args = 0, .max_args = 0, },
	{ .name = "set",              .abbr = "se",  .id = -1,      .descr = "descr",
	  .flags = HAS_COMMENT,
	  .handler = &call_cmd,       .min_args = 0, .max_args = NOT_DEF, },
	{ .name = "substitute",       .abbr = "s",   .id = -1,      .descr = "descr",
	  .flags = HAS_CUST_SEP | HAS_EMARK,
	  .handler = &substitute_cmd, .min_args = 0, .max_args = 3, },
	{ .name = "put",              .abbr = "pu",  .id = -1,      .descr = "descr",
	  .flags = HAS_EMARK | HAS_BG_FLAG,
	  .handler = &dummy_handler,  .min_args = 0, .max_args = 1, },
	{ .name = "quit",             .abbr = "q",   .id = -1,      .descr = "descr",
	  .flags = HAS_EMARK,
	  .handler = &quit_cmd,       .min_args = 0, .max_args = 0, },
};

SETUP()
{
	add_builtin_commands(commands, ARRAY_LEN(commands));
}

static int
dummy_handler(const cmd_info_t* cmd_info)
{
	return 0;
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
	cmdi = *cmd_info;
	cmdi.usr1 = 1;
	if(cmdi.argc > 0)
	{
		free(arg);
		arg = strdup(cmdi.argv[0]);
	}
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

TEST(incomplete_regexp_causes_error)
{
	assert_failure(execute_cmd("filter /te"));
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

TEST(non_printable_arg)
{
	/* \x0C is Ctrl-L. */
	assert_int_equal(0, execute_cmd("call \x0C"));
	assert_string_equal("\x0C", arg);
}

TEST(closing_double_quote_is_taken_as_comment)
{
	/* That's result of ambiguity of parsing, instead real :set doesn't have
	 * comments enabled for engine/cmds, but engine/set handles the comments. */
	assert_success(execute_cmd("set \"  string  \""));
	assert_string_equal("\"  string", arg);
}

TEST(background_mark_is_removed_properly_in_presence_of_a_quote)
{
	assert_success(execute_cmd("put \" &"));
}

TEST(escaping_allows_use_of_spaces)
{
	assert_success(execute_cmd("edit \\ something"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal(" something", arg);
}

TEST(escaping_is_ignored_for_raw_arguments)
{
	assert_success(execute_cmd("edia \\ something"));
	assert_int_equal(2, cmdi.argc);
	assert_string_equal("\\", arg);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
