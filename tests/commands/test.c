#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "seatest.h"

#include "../../src/engine/cmds.h"
#include "../../src/engine/completion.h"

cmd_info_t user_cmd_info;

static int complete_args(int id, const char *args, int argc, char **argv,
		int arg_pos);
static int swap_range(void);
static int resolve_mark(char mark);
static char * expand_macros(const char *str, int for_shell, int *usr1,
		int *usr2);
static char * expand_envvars(const char *str);
static void post(int id);
static void select_range(int id, const cmd_info_t *cmd_info);
static int skip_at_beginning(int id, const char *args);

void input_tests(void);
void command_name_tests(void);
void completion_tests(void);
void user_cmds_tests(void);
void ids_tests(void);
void builtin_tests(void);
void one_number_range(void);
void last_argument_tests(void);
void unescape_tests(void);

cmds_conf_t cmds_conf = {
	.complete_args = complete_args,
	.swap_range = swap_range,
	.resolve_mark = resolve_mark,
	.expand_macros = expand_macros,
	.expand_envvars = expand_envvars,
	.post = post,
	.select_range = select_range,
	.skip_at_beginning = skip_at_beginning,
};

void
all_tests(void)
{
	input_tests();
	command_name_tests();
	completion_tests();
	user_cmds_tests();
	ids_tests();
	builtin_tests();
	one_number_range();
	last_argument_tests();
	unescape_tests();
}

static int
complete_args(int id, const char *args, int argc, char **argv, int arg_pos)
{
	const char *arg;

	reset_completion();
	add_completion("followlinks");
	add_completion("fastrun");
	completion_group_end();
	add_completion("f");

	arg = strrchr(args, ' ');
	if(arg == NULL)
		arg = args;
	else
		arg++;
	return arg - args;
}

static int
swap_range(void)
{
	return 1;
}

static int
resolve_mark(char mark)
{
	if(isdigit(mark))
		return -1;
	return 75;
}

static char *
expand_macros(const char *str, int for_shell, int *usr1, int *usr2)
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

static void
setup(void)
{
	cmd_add_t command = {
		.name = "<USERCMD>", .abbr = NULL, .handler = usercmd_cmd, .cust_sep = 0,
		.id = -1,            .range = 1,   .emark = 0,             .qmark = 0,
		.expand = 0,         .regexp = 0,  .min_args = 0,          .max_args = 0,
	};

	cmds_conf.begin = 10;
	cmds_conf.current = 50;
	cmds_conf.end = 100;

	init_cmds(1, &cmds_conf);

	add_builtin_commands(&command, 1);
}

static void
teardown(void)
{
	reset_cmds();
}

int
main(int argc, char **argv)
{
	suite_setup(setup);
	suite_teardown(teardown);

	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
