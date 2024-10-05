#include <stic.h>

#include "../../src/engine/cmds.h"

static int dummy_cmd(const cmd_info_t *cmd_info);

TEST(post_name_no_emark)
{
	cmd_add_t command = {
	  .name = "test",        .abbr = NULL,  .id = -1,      .descr = "descr",
	  .flags = 0,
	  .handler = &dummy_cmd, .min_args = 0, .max_args = 1,
	};

	assert_success(add_builtin_cmd(command.name, BUILTIN_CMD, &command));

	cmd_info_t info;

	assert_non_null(vle_cmds_parse("test a", &info));
	assert_string_equal(" a", info.post_name);
	assert_string_equal("a", info.raw_args);

	assert_non_null(vle_cmds_parse("test !", &info));
	assert_string_equal(" !", info.post_name);
	assert_string_equal("!", info.raw_args);

	assert_non_null(vle_cmds_parse("test!", &info));
	assert_string_equal("!", info.post_name);
	assert_string_equal("!", info.raw_args);
}

TEST(post_name_emark)
{
	cmd_add_t command = {
	  .name = "test",        .abbr = NULL,  .id = -1,      .descr = "descr",
	  .flags = HAS_EMARK,
	  .handler = &dummy_cmd, .min_args = 0, .max_args = 1,
	};

	assert_success(add_builtin_cmd(command.name, BUILTIN_CMD, &command));

	cmd_info_t info;

	assert_non_null(vle_cmds_parse("test a", &info));
	assert_string_equal(" a", info.post_name);
	assert_string_equal("a", info.raw_args);

	assert_non_null(vle_cmds_parse("test !", &info));
	assert_string_equal(" !", info.post_name);
	assert_string_equal("!", info.raw_args);

	assert_non_null(vle_cmds_parse("test!", &info));
	assert_string_equal("", info.post_name);
	assert_string_equal("", info.raw_args);
}

static int
dummy_cmd(const cmd_info_t *cmd_info)
{
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
