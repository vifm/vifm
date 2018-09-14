#include <stic.h>

#include <string.h>

#include "../../src/engine/cmds.h"
#include "../../src/utils/macros.h"
#include "../../src/cmd_core.h"

extern cmds_conf_t cmds_conf;

static int wincmd_cmd(const cmd_info_t* cmd_info);

static cmd_info_t cmdi;

static const cmd_add_t commands[] = {
	{ .name = "wincmd",       .abbr = "winc", .id = -1,      .descr = "descr",
	  .flags = HAS_RANGE,
	  .handler = &wincmd_cmd, .min_args = 1,  .max_args = 1, },
};

SETUP()
{
	cmds_conf.begin = 0;
	cmds_conf.current = 10;
	cmds_conf.end = 20;

	cmdi.count = ~NOT_DEF;
	assert_false(cmdi.count == NOT_DEF);

	vle_cmds_add(commands, ARRAY_LEN(commands));
}

static int
wincmd_cmd(const cmd_info_t* cmd_info)
{
	cmdi = *cmd_info;
	return 0;
}

TEST(count_is_undef_if_not_present)
{
	assert_success(vle_cmds_run("wincmd a"));
	assert_int_equal(NOT_DEF, cmdi.count);
}

TEST(count_can_be_larger_than_end)
{
	assert_success(vle_cmds_run("40wincmd a"));
	assert_int_equal(40, cmdi.count);
}

TEST(count_can_be_negative)
{
	assert_success(vle_cmds_run("-11wincmd a"));
	assert_int_equal(0, cmdi.count);

	assert_success(vle_cmds_run("-40wincmd a"));
	assert_int_equal(cmds_conf.current - 40 + 1, cmdi.count);
}

TEST(special_values_are_valid_count)
{
	assert_success(vle_cmds_run(".wincmd a"));
	assert_int_equal(cmds_conf.current + 1, cmdi.count);

	assert_success(vle_cmds_run("$wincmd a"));
	assert_int_equal(cmds_conf.end - cmds_conf.begin + 1, cmdi.count);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
