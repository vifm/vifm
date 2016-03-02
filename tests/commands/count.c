#include <stic.h>

#include <string.h>

#include "../../src/engine/cmds.h"
#include "../../src/utils/macros.h"
#include "../../src/cmd_core.h"

extern cmds_conf_t cmds_conf;

static int wincmd_cmd(const cmd_info_t* cmd_info);

static cmd_info_t cmdi;

static const cmd_add_t commands[] = {
	{ .name = "wincmd", .abbr = "winc", .handler = &wincmd_cmd, .id = -1,    .range = 1,    .cust_sep = 0, .descr = "descr",
		.emark = 0,       .qmark = 0,     .expand = 0,            .regexp = 0, .min_args = 1, .max_args = 1, .bg = 0,     },
};

SETUP()
{
	cmds_conf.begin = 0;
	cmds_conf.current = 10;
	cmds_conf.end = 20;

	cmdi.count = ~NOT_DEF;
	assert_false(cmdi.count == NOT_DEF);

	add_builtin_commands(commands, ARRAY_LEN(commands));
}

static int
wincmd_cmd(const cmd_info_t* cmd_info)
{
	cmdi = *cmd_info;
	return 0;
}

TEST(count_is_undef_if_not_present)
{
	assert_success(execute_cmd("wincmd a"));
	assert_int_equal(NOT_DEF, cmdi.count);
}

TEST(count_can_be_larger_than_end)
{
	assert_success(execute_cmd("40wincmd a"));
	assert_int_equal(40, cmdi.count);
}

TEST(count_can_be_negative)
{
	assert_success(execute_cmd("-11wincmd a"));
	assert_int_equal(0, cmdi.count);

	assert_success(execute_cmd("-40wincmd a"));
	assert_int_equal(cmds_conf.current - 40 + 1, cmdi.count);
}

TEST(special_values_are_valid_count)
{
	assert_success(execute_cmd(".wincmd a"));
	assert_int_equal(cmds_conf.current + 1, cmdi.count);

	assert_success(execute_cmd("$wincmd a"));
	assert_int_equal(cmds_conf.end - cmds_conf.begin + 1, cmdi.count);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
