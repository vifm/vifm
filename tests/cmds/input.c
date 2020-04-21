#include <stic.h>

#include <stdlib.h>
#include <string.h>

#include "../../src/engine/cmds.h"
#include "../../src/utils/macros.h"

#include "suite.h"

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
	vle_cmds_add(commands, ARRAY_LEN(commands));
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
	assert_int_equal(0, vle_cmds_run("q"));
	assert_int_equal(0, vle_cmds_run(" q"));
	assert_int_equal(0, vle_cmds_run("   q"));
	assert_int_equal(0, vle_cmds_run("\tq"));
	assert_int_equal(0, vle_cmds_run("\t\tq"));
	assert_int_equal(0, vle_cmds_run("\t\t q"));
	assert_int_equal(0, vle_cmds_run(" \t \tq"));
	assert_int_equal(0, vle_cmds_run("\t \tq"));
}

TEST(range_acceptance)
{
	assert_int_equal(0, vle_cmds_run("%delete"));
	assert_int_equal(0, vle_cmds_run(",%delete"));
	assert_int_equal(0, vle_cmds_run(";%delete"));
	assert_int_equal(CMDS_ERR_NO_RANGE_ALLOWED, vle_cmds_run("%history"));
	assert_int_equal(CMDS_ERR_NO_RANGE_ALLOWED, vle_cmds_run("%,history"));
	assert_int_equal(CMDS_ERR_NO_RANGE_ALLOWED, vle_cmds_run("%;history"));
}

TEST(single_quote_doubling)
{
	assert_int_equal(0, vle_cmds_run("delete 'file''with''single''quotes'"));
	assert_string_equal("file'with'single'quotes", arg);
}

TEST(range)
{
	cmds_conf.begin = 0;
	cmds_conf.current = 50;
	cmds_conf.end = 100;

	assert_int_equal(0, vle_cmds_run("%delete!"));
	assert_int_equal(0, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, vle_cmds_run("$delete!"));
	assert_int_equal(100, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, vle_cmds_run(".,$delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, vle_cmds_run(",$delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, vle_cmds_run("$,.delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, vle_cmds_run("$,delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, vle_cmds_run(",delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(50, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, vle_cmds_run("5,6,7delete!"));
	assert_int_equal(5, cmdi.begin);
	assert_int_equal(6, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, vle_cmds_run("1,3,7"));
	assert_int_equal(2, cmdi.begin);
	assert_int_equal(6, cmdi.end);

	assert_int_equal(0, vle_cmds_run("7,3,1"));
	assert_int_equal(2, cmdi.begin);
	assert_int_equal(0, cmdi.end);
}

TEST(semicolon_range)
{
	cmds_conf.begin = 0;
	cmds_conf.current = 50;
	cmds_conf.end = 100;

	assert_int_equal(0, vle_cmds_run(".;$delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, vle_cmds_run(";$delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, vle_cmds_run("$;delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, vle_cmds_run(";;delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(50, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, vle_cmds_run("5;6;7delete!"));
	assert_int_equal(5, cmdi.begin);
	assert_int_equal(6, cmdi.end);
	assert_true(cmdi.emark);
}

TEST(range_plus_minus)
{
	cmds_conf.begin = 0;
	cmds_conf.current = 50;
	cmds_conf.end = 100;

	assert_int_equal(0, vle_cmds_run(".+"));
	assert_int_equal(51, cmdi.begin);
	assert_int_equal(51, cmdi.end);

	assert_int_equal(0, vle_cmds_run("+2"));
	assert_int_equal(52, cmdi.begin);
	assert_int_equal(52, cmdi.end);

	assert_int_equal(0, vle_cmds_run(".++"));
	assert_int_equal(52, cmdi.begin);
	assert_int_equal(52, cmdi.end);

	assert_int_equal(0, vle_cmds_run(".+0-0"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(50, cmdi.end);

	assert_int_equal(0, vle_cmds_run(".+-"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(50, cmdi.end);

	assert_int_equal(0, vle_cmds_run(".+5-2"));
	assert_int_equal(53, cmdi.begin);
	assert_int_equal(53, cmdi.end);

	assert_int_equal(0, vle_cmds_run(".,.+500"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);

	assert_int_equal(0, vle_cmds_run(".,.-500"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(0, cmdi.end);
}

TEST(range_comma_vs_semicolon_bases)
{
	cmds_conf.begin = 0;
	cmds_conf.current = 0;
	cmds_conf.end = 3;

	assert_int_equal(0, vle_cmds_run("2,+2"));
	assert_int_equal(1, cmdi.begin);
	assert_int_equal(2, cmdi.end);

	assert_int_equal(0, vle_cmds_run("2;+2"));
	assert_int_equal(1, cmdi.begin);
	assert_int_equal(3, cmdi.end);
}

TEST(range_and_spaces)
{
	cmds_conf.begin = 10;
	cmds_conf.current = 50;
	cmds_conf.end = 100;

	assert_int_equal(0, vle_cmds_run("% delete!"));
	assert_int_equal(10, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, vle_cmds_run("  ,  ,  ,   $  ,   .   delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);
}

TEST(empty_range_empty_command_called)
{
	cmds_conf.begin = 10;
	cmds_conf.current = 50;
	cmds_conf.end = 100;

	assert_int_equal(0, vle_cmds_run(""));
	assert_int_equal(NOT_DEF, cmdi.begin);
	assert_int_equal(NOT_DEF, cmdi.end);

	assert_int_equal(0, vle_cmds_run(","));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(50, cmdi.end);

	assert_int_equal(0, vle_cmds_run(";"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(50, cmdi.end);
}

TEST(bang_acceptance)
{
	assert_int_equal(0, vle_cmds_run("q"));
	assert_int_equal(CMDS_ERR_NO_BANG_ALLOWED, vle_cmds_run("history!"));
}

TEST(bang)
{
	assert_int_equal(0, vle_cmds_run("q"));
	assert_false(cmdi.emark);

	assert_int_equal(0, vle_cmds_run("q!"));
	assert_true(cmdi.emark);

	assert_int_equal(0, vle_cmds_run("!vim"));
	assert_false(cmdi.emark);

	assert_int_equal(0, vle_cmds_run("!!vim"));
	assert_true(cmdi.emark);

	assert_int_equal(CMDS_ERR_TRAILING_CHARS, vle_cmds_run("q !"));
}

TEST(qmark_acceptance)
{
	assert_int_equal(0, vle_cmds_run("invert?"));
	assert_int_equal(CMDS_ERR_NO_QMARK_ALLOWED, vle_cmds_run("history?"));
}

TEST(qmark)
{
	assert_int_equal(0, vle_cmds_run("invert"));
	assert_false(cmdi.qmark);

	assert_int_equal(0, vle_cmds_run("invert?"));
	assert_true(cmdi.qmark);

	assert_int_equal(CMDS_ERR_TRAILING_CHARS, vle_cmds_run("invert ?"));
}

TEST(args)
{
	assert_int_equal(CMDS_ERR_TOO_FEW_ARGS, vle_cmds_run("call"));
	assert_int_equal(CMDS_ERR_TRAILING_CHARS, vle_cmds_run("call a b"));

	assert_int_equal(0, vle_cmds_run("call a"));
	assert_int_equal(1, cmdi.argc);

	assert_int_equal(0, vle_cmds_run("delete"));
	assert_int_equal(0, cmdi.argc);

	assert_int_equal(0, vle_cmds_run("edit a b c d"));
	assert_int_equal(4, cmdi.argc);
}

TEST(count)
{
	assert_int_equal(0, vle_cmds_run("delete 10"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("10", arg);

	assert_int_equal(0, vle_cmds_run("delete10"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("10", arg);
}

TEST(end_characters)
{
	assert_int_equal(0, vle_cmds_run("call a"));
	assert_int_equal(CMDS_ERR_TRAILING_CHARS, vle_cmds_run("call, a"));
}

TEST(custom_separator)
{
	assert_int_equal(0, vle_cmds_run("s"));
	assert_int_equal(0, cmdi.argc);
	assert_int_equal(0, vle_cmds_run("s/some"));
	assert_int_equal(1, cmdi.argc);
	assert_int_equal(0, vle_cmds_run("s/some/thing"));
	assert_int_equal(2, cmdi.argc);
	assert_int_equal(0, vle_cmds_run("s/some/thing"));
	assert_int_equal(2, cmdi.argc);
	assert_int_equal(0, vle_cmds_run("s/some/thing/g"));
	assert_int_equal(3, cmdi.argc);
	assert_int_equal(CMDS_ERR_TRAILING_CHARS, vle_cmds_run("s/some/thing/g/j"));
}

TEST(custom_separator_and_arg_format)
{
	assert_int_equal(0, vle_cmds_run("s/\"so\"me\"/thing/g"));
	assert_int_equal(3, cmdi.argc);
	assert_string_equal("\"so\"me\"", arg);

	assert_int_equal(0, vle_cmds_run("s/'some'/thing/g"));
	assert_int_equal(3, cmdi.argc);
	assert_string_equal("'some'", arg);
}

TEST(custom_separator_and_emark)
{
	assert_int_equal(0, vle_cmds_run("s!"));
	assert_int_equal(0, cmdi.argc);
	assert_true(cmdi.emark);

	assert_int_equal(0, vle_cmds_run("s!/some"));
	assert_int_equal(1, cmdi.argc);
	assert_true(cmdi.emark);

	assert_int_equal(0, vle_cmds_run("s!/some/thing"));
	assert_int_equal(2, cmdi.argc);
	assert_true(cmdi.emark);

	assert_int_equal(0, vle_cmds_run("s!/some/thing/g"));
	assert_int_equal(3, cmdi.argc);
	assert_true(cmdi.emark);

	assert_int_equal(CMDS_ERR_TRAILING_CHARS, vle_cmds_run("s!/some/thing/g/j"));
}

TEST(incomplete_regexp_causes_error)
{
	assert_failure(vle_cmds_run("filter /te"));
}

TEST(regexp_flag_strips_slashes)
{
	assert_int_equal(0, vle_cmds_run("e /te|xt/"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("/te|xt/", arg);

	assert_int_equal(0, vle_cmds_run("filter /te|xt/"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("te|xt", arg);
}

TEST(regexp_flag_slashes_and_spaces)
{
	assert_int_equal(0, vle_cmds_run("filter /te|xt/i"));
	assert_int_equal(2, cmdi.argc);
	assert_string_equal("te|xt", arg);

	assert_int_equal(0, vle_cmds_run("filter/te|xt/i"));
	assert_int_equal(2, cmdi.argc);
	assert_string_equal("te|xt", arg);
}

TEST(regexp_flag_bang_slashes_and_spaces)
{
	assert_int_equal(0, vle_cmds_run("filter! /te|xt/i"));
	assert_int_equal(2, cmdi.argc);
	assert_string_equal("te|xt", arg);

	assert_int_equal(0, vle_cmds_run("filter!/te|xt/i"));
	assert_int_equal(2, cmdi.argc);
	assert_string_equal("te|xt", arg);
}

TEST(backgrounding)
{
	assert_int_equal(0, vle_cmds_run("e &"));
	assert_int_equal(1, cmdi.argc);
	assert_int_equal(0, cmdi.bg);

	assert_int_equal(0, vle_cmds_run("!vim&"));
	assert_int_equal(1, cmdi.argc);
	assert_int_equal(0, cmdi.bg);

	assert_int_equal(0, vle_cmds_run("!vim &"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("vim", arg);
	assert_int_equal(1, cmdi.bg);
}

TEST(no_args_after_qmark_1)
{
	assert_int_equal(0, vle_cmds_run("filter?"));
	assert_int_equal(0, vle_cmds_run("filter?      "));
	assert_int_equal(CMDS_ERR_TRAILING_CHARS, vle_cmds_run("filter? some_thing"));
}

TEST(args_after_qmark_2)
{
	assert_int_equal(0, vle_cmds_run("call? arg"));
	assert_int_equal(0, vle_cmds_run("call? some_thing"));
}

TEST(no_space_before_e_and_q_marks)
{
	assert_int_equal(0, vle_cmds_run("filter?"));
	assert_int_equal(cmdi.argc, 0);
	assert_true(cmdi.qmark);

	assert_int_equal(0, vle_cmds_run("filter ?"));
	assert_int_equal(cmdi.argc, 1);
	assert_false(cmdi.qmark);
}

TEST(only_one_mark)
{
	assert_int_equal(0, vle_cmds_run("filter?"));
	assert_int_equal(cmdi.argc, 0);
	assert_true(cmdi.qmark);

	assert_int_equal(0, vle_cmds_run("filter!"));
	assert_int_equal(cmdi.argc, 0);
	assert_true(cmdi.emark);

	assert_int_equal(0, vle_cmds_run("filter!!"));
	assert_int_equal(cmdi.argc, 1);
	assert_true(cmdi.emark);

	assert_int_equal(CMDS_ERR_TRAILING_CHARS, vle_cmds_run("filter?!"));
	assert_int_equal(CMDS_ERR_TRAILING_CHARS, vle_cmds_run("filter??"));

	assert_int_equal(0, vle_cmds_run("filter!?"));
	assert_int_equal(cmdi.argc, 1);
	assert_true(cmdi.emark);
	assert_false(cmdi.qmark);
}

TEST(args_whitespace_trimming)
{
	assert_int_equal(0, vle_cmds_run("call hi"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("hi", arg);

	assert_int_equal(0, vle_cmds_run("call\t'hi'"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("'hi'", arg);

	assert_int_equal(0, vle_cmds_run("call\t hi "));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("hi", arg);

	assert_int_equal(0, vle_cmds_run("call hi \t"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("hi", arg);

	assert_int_equal(0, vle_cmds_run("call\t \thi\\  "));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("hi\\ ", arg);
}

TEST(bg_and_no_args)
{
	assert_int_equal(0, vle_cmds_run("file"));
	assert_false(cmdi.bg);

	assert_int_equal(0, vle_cmds_run("file &"));
	assert_true(cmdi.bg);
}

TEST(bg_followed_by_whitespace)
{
	assert_int_equal(0, vle_cmds_run("file & "));
	assert_true(cmdi.bg);

	assert_int_equal(0, vle_cmds_run("file &  "));
	assert_true(cmdi.bg);

	assert_int_equal(0, vle_cmds_run("file& "));
	assert_true(cmdi.bg);

	assert_int_equal(0, vle_cmds_run("file&  "));
	assert_true(cmdi.bg);
}

TEST(short_forms)
{
	assert_int_equal(0, vle_cmds_run("e"));
	assert_int_equal(2, cmdi.usr1);

	assert_int_equal(0, vle_cmds_run("ed"));
	assert_int_equal(2, cmdi.usr1);

	assert_int_equal(0, vle_cmds_run("edi"));
	assert_int_equal(2, cmdi.usr1);

	assert_int_equal(0, vle_cmds_run("edit"));
	assert_int_equal(2, cmdi.usr1);

	assert_int_equal(0, vle_cmds_run("edia"));
	assert_int_equal(1, cmdi.usr1);
}

TEST(qmark_and_bg)
{
	assert_int_equal(0, vle_cmds_run("file &"));
	assert_true(cmdi.bg);

	assert_int_equal(0, vle_cmds_run("file?"));
	assert_true(cmdi.qmark);

	assert_int_equal(0, vle_cmds_run("file? &"));
	assert_true(cmdi.bg);
	assert_true(cmdi.qmark);
}

TEST(extra_long_command_name)
{
	char cmd_name[1024];

	memset(cmd_name, 'a', sizeof(cmd_name) - 1);
	cmd_name[sizeof(cmd_name) - 1] = '\0';

	assert_false(vle_cmds_run(cmd_name) == 0);
}

TEST(emark_is_checked_before_number_of_args)
{
	assert_int_equal(CMDS_ERR_NO_BANG_ALLOWED, vle_cmds_run("call!"));
}

TEST(qmark_is_checked_before_number_of_args)
{
	assert_int_equal(CMDS_ERR_NO_QMARK_ALLOWED, vle_cmds_run("quit? from here"));
}

TEST(missing_quotes_are_allowed)
{
	assert_int_equal(CMDS_ERR_INVALID_ARG, vle_cmds_run("call 'ismissing"));
}

TEST(non_printable_arg)
{
	/* \x0C is Ctrl-L. */
	assert_int_equal(0, vle_cmds_run("call \x0C"));
	assert_string_equal("\x0C", arg);
}

TEST(closing_double_quote_is_taken_as_comment)
{
	/* That's result of ambiguity of parsing, instead real :set doesn't have
	 * comments enabled for engine/cmds, but engine/set handles the comments. */
	assert_success(vle_cmds_run("set \"  string  \""));
	assert_string_equal("\"  string", arg);
}

TEST(background_mark_is_removed_properly_in_presence_of_a_quote)
{
	assert_success(vle_cmds_run("put \" &"));
}

TEST(escaping_allows_use_of_spaces)
{
	assert_success(vle_cmds_run("edit \\ something"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal(" something", arg);
}

TEST(escaping_is_ignored_for_raw_arguments)
{
	assert_success(vle_cmds_run("edia \\ something"));
	assert_int_equal(2, cmdi.argc);
	assert_string_equal("\\", arg);
}

TEST(bad_mark)
{
	assert_int_equal(CMDS_ERR_INVALID_RANGE, vle_cmds_run("'1,'2file"));
}

TEST(bad_range)
{
	assert_int_equal(CMDS_ERR_INVALID_CMD, vle_cmds_run("#file"));
}

TEST(inversed_range)
{
	swap_range = 0;
	assert_int_equal(CMDS_ERR_INVALID_RANGE, vle_cmds_run("20,10file"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
