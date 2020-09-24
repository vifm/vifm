#include <stic.h>

#include "../../src/engine/cmds.h"

static int cmd_cmd(const cmd_info_t *cmd_info);

static int data;

TEST(user_data_is_passed_to_handler)
{
	cmd_add_t command = {
	  .name = "cmd",           .abbr = NULL,  .id = -1,            .descr = "cmd",
	  .flags = HAS_RANGE,
	  .handler = &cmd_cmd, .min_args = 0, .max_args = NOT_DEF,
		.user_data = &data
	};
	vle_cmds_add(&command, 1);

	assert_success(vle_cmds_run("cmd"));
}

static int
cmd_cmd(const cmd_info_t *cmd_info)
{
	return (cmd_info->user_data == &data ? 0 : 1);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
