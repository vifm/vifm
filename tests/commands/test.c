#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "seatest.h"

#include "../../src/cmds.h"

struct cmd_info user_cmd_info;

void input_tests(void);
void command_name_tests(void);
void completion_tests(void);
void user_cmds_tests(void);
void ids_tests(void);

void
all_tests(void)
{
	input_tests();
	command_name_tests();
	completion_tests();
	user_cmds_tests();
	ids_tests();
}

static void
complete_args(int id, const char *arg, struct complete_t *info)
{
	info->count = 2;
	info->buf = malloc(sizeof(char*)*2);
	info->buf[0] = strdup("fastrun");
	info->buf[1] = strdup("followlinks");
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
expand_macros(const char *str)
{
	return strdup(str);
}

static int
usercmd_cmd(const struct cmd_info* cmd_info)
{
	user_cmd_info = *cmd_info;
	return 0;
}

static void
post(int id)
{
}

static void
select_range(const struct cmd_info *cmd_info)
{
}

static void
setup(void)
{
	struct cmd_add command = {
		.name = "<USERCMD>", .abbr = NULL, .handler = usercmd_cmd, .cust_sep = 0,
		.id = -1,            .range = 1,   .emark = 0,             .qmark = 0,
		.expand = 0,         .regexp = 0,  .min_args = 0,          .max_args = 0,
	};

	cmds_conf.begin = 10;
	cmds_conf.current = 50;
	cmds_conf.end = 100;

	cmds_conf.complete_args = complete_args;
	cmds_conf.swap_range = swap_range;
	cmds_conf.resolve_mark = resolve_mark;
	cmds_conf.expand_macros = expand_macros;
	cmds_conf.post = post;
	cmds_conf.select_range = select_range;

	init_cmds();

	add_buildin_commands(&command, 1);
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
