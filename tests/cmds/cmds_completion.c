#include <stic.h>

#include <stddef.h> /* NULL */
#include <stdlib.h>
#include <string.h>

#include "../../src/engine/cmds.h"
#include "../../src/engine/completion.h"
#include "../../src/utils/macros.h"

enum { COM_WINDO };

static int dummy_cmd(const cmd_info_t *cmd_info);
static int delete_cmd(const cmd_info_t *cmd_info);
static int skip_at_beginning(int id, const char *args);

extern cmds_conf_t cmds_conf;

static cmd_info_t cmdi;

static const cmd_add_t commands[] = {
	{ .name = "",             .abbr = NULL,  .id = -1,           .descr = "descr",
	  .flags = HAS_RANGE,
	  .handler = &dummy_cmd,  .min_args = 0, .max_args = 0, },
	{ .name = "delete",       .abbr = "d",   .id = 1,            .descr = "descr",
	  .flags = HAS_RANGE | HAS_EMARK,
	  .handler = &delete_cmd, .min_args = 0, .max_args = 1, },
	{ .name = "yank",         .abbr = "y",   .id = -10,          .descr = "descr",
	  .flags = 0,
	  .handler = &dummy_cmd,  .min_args = 0, .max_args = 1, },
	{ .name = "windo",        .abbr = NULL,  .id = COM_WINDO,    .descr = "descr",
	  .flags = 0,
	  .handler = &dummy_cmd,  .min_args = 0, .max_args = NOT_DEF, },
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

SETUP()
{
	vle_cmds_add(commands, ARRAY_LEN(commands));
	cmds_conf.skip_at_beginning = &skip_at_beginning;
}

TEST(skip_goto)
{
	char *completion;

	vle_compl_reset();
	assert_int_equal(0, vle_cmds_complete("", NULL));
	assert_int_equal(7, vle_compl_get_count());
	if(vle_compl_get_count() == 0)
		return;
	completion = vle_compl_next();
	assert_string_equal("comclear", completion);
	free(completion);
	completion = vle_compl_next();
	assert_string_equal("command", completion);
	free(completion);
	completion = vle_compl_next();
	assert_string_equal("delcommand", completion);
	free(completion);
}

TEST(skip_abbreviations)
{
	char *completion;

	vle_compl_reset();
	assert_int_equal(0, vle_cmds_complete("d", NULL));
	assert_int_equal(3, vle_compl_get_count());
	if(vle_compl_get_count() == 0)
		return;
	completion = vle_compl_next();
	assert_string_equal("delcommand", completion);
	free(completion);
	completion = vle_compl_next();
	assert_string_equal("delete", completion);
	free(completion);
}

TEST(dont_remove_range)
{
	char *completion;

	vle_compl_reset();
	assert_int_equal(2, vle_cmds_complete("% del", NULL));
	assert_int_equal(3, vle_compl_get_count());
	if(vle_compl_get_count() == 0)
		return;
	completion = vle_compl_next();
	assert_string_equal("delcommand", completion);
	free(completion);

	vle_compl_reset();
	assert_int_equal(1, vle_cmds_complete("3del", NULL));
	assert_int_equal(3, vle_compl_get_count());
	if(vle_compl_get_count() == 0)
		return;
	completion = vle_compl_next();
	assert_string_equal("delcommand", completion);
	free(completion);
}

TEST(dont_remove_cmd)
{
	char *completion;

	vle_compl_reset();
	assert_int_equal(7, vle_cmds_complete("% dele ", NULL));
	assert_int_equal(3, vle_compl_get_count());
	if(vle_compl_get_count() == 0)
		return;
	completion = vle_compl_next();
	assert_string_equal("fastrun", completion);
	free(completion);
	completion = vle_compl_next();
	assert_string_equal("followlinks", completion);
	free(completion);
}

TEST(skip_complete_args)
{
	char *completion;

	vle_compl_reset();
	assert_int_equal(17, vle_cmds_complete("% dele fast slow ", NULL));
	assert_int_equal(3, vle_compl_get_count());
	if(vle_compl_get_count() == 0)
		return;
	completion = vle_compl_next();
	assert_string_equal("fastrun", completion);
	free(completion);
	completion = vle_compl_next();
	assert_string_equal("followlinks", completion);
	free(completion);
}

TEST(com_completion)
{
	char *completion;

	assert_int_equal(0, vle_cmds_run("command udf a"));

	vle_compl_reset();
	assert_int_equal(4, vle_cmds_complete("com u", NULL));
	assert_int_equal(2, vle_compl_get_count());
	if(vle_compl_get_count() == 0)
		return;
	completion = vle_compl_next();
	assert_string_equal("udf", completion);
	free(completion);
}

TEST(delc_completion)
{
	char *completion;

	assert_int_equal(0, vle_cmds_run("command udf a"));

	vle_compl_reset();
	assert_int_equal(5, vle_cmds_complete("delc u", NULL));
	assert_int_equal(2, vle_compl_get_count());
	if(vle_compl_get_count() == 0)
		return;
	completion = vle_compl_next();
	assert_string_equal("udf", completion);
	free(completion);
}

TEST(windo_completion)
{
	char *completion;

	vle_compl_reset();
	assert_int_equal(0, vle_cmds_complete("win", NULL));
	assert_int_equal(2, vle_compl_get_count());
	if(vle_compl_get_count() == 0)
		return;
	completion = vle_compl_next();
	assert_string_equal("windo", completion);
	free(completion);
}

TEST(windo_windo_completion)
{
	char *completion;

	vle_compl_reset();
	assert_int_equal(12, vle_cmds_complete("windo windo ", NULL));
	assert_int_equal(7, vle_compl_get_count());
	if(vle_compl_get_count() == 0)
		return;
	completion = vle_compl_next();
	assert_string_equal("comclear", completion);
	free(completion);
}

TEST(broken_range_after_prefix_breaks_completion)
{
	vle_compl_reset();
	assert_int_equal(5, vle_cmds_complete("windo #windo ", NULL));
	assert_int_equal(0, vle_compl_get_count());
}

TEST(windo_args_completion)
{
	char *completion;

	vle_compl_reset();
	assert_int_equal(6, vle_cmds_complete("windo del", NULL));
	assert_int_equal(3, vle_compl_get_count());
	if(vle_compl_get_count() == 0)
		return;
	completion = vle_compl_next();
	assert_string_equal("delcommand", completion);
	free(completion);
	completion = vle_compl_next();
	assert_string_equal("delete", completion);
	free(completion);
}

TEST(no_completion_for_negative_ids)
{
	vle_compl_reset();
	assert_int_equal(4, vle_cmds_complete("yank ", NULL));
	assert_int_equal(0, vle_compl_get_count());
}

TEST(udf_completion)
{
	char *buf;

	vle_cmds_run("command bar a");
	vle_cmds_run("command baz b");
	vle_cmds_run("command foo c");

	vle_compl_reset();
	assert_int_equal(5, vle_cmds_complete("comm b", NULL));
	buf = vle_compl_next();
	assert_string_equal("bar", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("baz", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("b", buf);
	free(buf);

	vle_compl_reset();
	assert_int_equal(5, vle_cmds_complete("comm f", NULL));
	buf = vle_compl_next();
	assert_string_equal("foo", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("foo", buf);
	free(buf);

	vle_compl_reset();
	assert_int_equal(5, vle_cmds_complete("comm b", NULL));
	buf = vle_compl_next();
	assert_string_equal("bar", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("baz", buf);
	free(buf);
}

TEST(completing_args_of_unknown_command)
{
	vle_compl_reset();
	assert_int_equal(0, vle_cmds_complete("unknown ", NULL));
}

TEST(line_completion_no_args)
{
	char *buf;

	vle_compl_reset();
	assert_int_equal(0, vle_cmds_complete("notreallyacommand", NULL));

	buf = vle_compl_next();
	assert_string_equal("whole-line1", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("whole-line2", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("notreallyacommand", buf);
	free(buf);
}

TEST(line_completion_args)
{
	char *buf;

	vle_compl_reset();
	assert_int_equal(0, vle_cmds_complete("notreally a command", NULL));

	buf = vle_compl_next();
	assert_string_equal("whole-line1", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("whole-line2", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("notreally a command", buf);
	free(buf);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
