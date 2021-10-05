#include <stic.h>

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <test-utils.h>

#include "../../src/engine/cmds.h"
#include "../../src/engine/completion.h"

#include "suite.h"

static int complete_line(const char cmd_line[], void *extra_arg);
static int complete_args(int id, const cmd_info_t *cmd_info, int arg_pos,
		void *extra_arg);
static int should_swap_range(void);
static int resolve_mark(char mark);
static char * expand_macros(const char str[], int for_shell, int *usr1,
		int *usr2);
static char * expand_envvars(const char *str);
static int usercmd_cmd(const cmd_info_t *cmd_info);
static int def_usercmd_cmd(const cmd_info_t *cmd_info);
static void post(int id);
static void select_range(int id, const cmd_info_t *cmd_info);
static int skip_at_beginning(int id, const char *args);

cmd_info_t user_cmd_info;
cmd_handler user_cmd_handler;

cmds_conf_t cmds_conf = {
	.complete_line = &complete_line,
	.complete_args = &complete_args,
	.swap_range = &should_swap_range,
	.resolve_mark = &resolve_mark,
	.expand_macros = &expand_macros,
	.expand_envvars = &expand_envvars,
	.post = &post,
	.select_range = &select_range,
	.skip_at_beginning = &skip_at_beginning,
};

int swap_range;

DEFINE_SUITE();

SETUP_ONCE()
{
	fix_environ();
}

SETUP()
{
	cmd_add_t command = {
	  .name = "<USERCMD>",     .abbr = NULL,  .id = -1,      .descr = "descr",
	  .flags = HAS_RANGE,
	  .handler = &usercmd_cmd, .min_args = 0, .max_args = NOT_DEF,
	};

	cmds_conf.begin = 10;
	cmds_conf.current = 50;
	cmds_conf.end = 100;

	vle_cmds_init(1, &cmds_conf);

	vle_cmds_add(&command, 1);

	swap_range = 1;
	user_cmd_handler = &def_usercmd_cmd;
}

TEARDOWN()
{
	vle_cmds_reset();
}

static int
complete_line(const char cmd_line[], void *extra_arg)
{
	vle_compl_reset();
	vle_compl_add_match("whole-line1", "");
	vle_compl_add_match("whole-line2", "");
	vle_compl_finish_group();
	vle_compl_add_last_match(cmd_line);
	return 0;
}

static int
complete_args(int id, const cmd_info_t *cmd_info, int arg_pos, void *extra_arg)
{
	const char *arg;

	vle_compl_reset();
	vle_compl_add_match("followlinks", "");
	vle_compl_add_match("fastrun", "");
	vle_compl_finish_group();
	vle_compl_add_last_match("f");

	arg = strrchr(cmd_info->args, ' ');
	if(arg == NULL)
		arg = cmd_info->args;
	else
		arg++;
	return arg - cmd_info->args;
}

static int
should_swap_range(void)
{
	return swap_range;
}

static int
resolve_mark(char mark)
{
	if(isdigit(mark))
		return -1;
	return 75;
}

static char *
expand_macros(const char str[], int for_shell, int *usr1, int *usr2)
{
	return strdup(str);
}

static char *
expand_envvars(const char *str)
{
	return strdup(str);
}

static int
usercmd_cmd(const cmd_info_t *cmd_info)
{
	return user_cmd_handler(cmd_info);
}

static int
def_usercmd_cmd(const cmd_info_t *cmd_info)
{
	user_cmd_info = *cmd_info;
	return 0;
}

static void
post(int id)
{
}

static void
select_range(int id, const cmd_info_t *cmd_info)
{
}

static int
skip_at_beginning(int id, const char *args)
{
	return -1;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
