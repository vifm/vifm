#include <stic.h>

#include <stddef.h> /* NULL */

#include "../../src/engine/cmds.h"
#include "../../src/engine/completion.h"
#include "../../src/utils/string_array.h"

static int foreign_cmd(const cmd_info_t *cmd_info);

TEST(cannot_add_foreign_command_with_bad_name)
{
	cmd_add_t command = {
	  .name = "some##",        .abbr = NULL,  .id = -1,      .descr = "foreign",
	  .flags = HAS_RANGE,
	  .handler = &foreign_cmd,
	};

	assert_failure(vle_cmds_add_foreign(&command));
}

TEST(cannot_add_foreign_command_with_bad_args)
{
	cmd_add_t command = {
	  .name = "foreign",       .abbr = NULL,  .id = -1,      .descr = "foreign",
	  .flags = HAS_RANGE,
	  .handler = &foreign_cmd,
	};

	command.min_args = -1;
	assert_failure(vle_cmds_add_foreign(&command));

	command.min_args = 2;
	command.max_args = 1;
	assert_failure(vle_cmds_add_foreign(&command));
}

TEST(cannot_add_duplicate_foreign_command)
{
	cmd_add_t command = {
	  .name = "comclear",      .abbr = NULL,  .id = -1,      .descr = "foreign",
	  .flags = HAS_RANGE,
	  .handler = &foreign_cmd, .min_args = 0, .max_args = NOT_DEF,
	};

	assert_failure(vle_cmds_add_foreign(&command));
}

TEST(can_add_foreign_command)
{
	cmd_add_t command = {
	  .name = "foreign",       .abbr = NULL,  .id = -1,      .descr = "foreign",
	  .flags = HAS_RANGE,
	  .handler = &foreign_cmd, .min_args = 0, .max_args = NOT_DEF,
	};

	assert_success(vle_cmds_add_foreign(&command));
}

TEST(foreign_command_is_not_gone_after_comclear)
{
	cmd_add_t command = {
	  .name = "foreign",       .abbr = NULL,  .id = -1,      .descr = "foreign",
	  .flags = HAS_RANGE,
	  .handler = &foreign_cmd, .min_args = 0, .max_args = NOT_DEF,
	};

	assert_success(vle_cmds_add_foreign(&command));
	assert_success(vle_cmds_run("comclear"));
	assert_failure(vle_cmds_add_foreign(&command));
}

TEST(foreign_command_is_gone_after_cmds_clear)
{
	cmd_add_t command = {
	  .name = "foreign",       .abbr = NULL,  .id = -1,      .descr = "foreign",
	  .flags = HAS_RANGE,
	  .handler = &foreign_cmd, .min_args = 0, .max_args = NOT_DEF,
	};

	assert_success(vle_cmds_add_foreign(&command));
	vle_cmds_clear();
	assert_success(vle_cmds_add_foreign(&command));
}

TEST(foreign_command_is_listed_with_udcs)
{
	cmd_add_t command = {
	  .name = "foreign",       .abbr = NULL,  .id = -1,      .descr = "descr",
	  .flags = HAS_RANGE,
	  .handler = &foreign_cmd, .min_args = 0, .max_args = NOT_DEF,
	};
	assert_success(vle_cmds_add_foreign(&command));

	char **list = vle_cmds_list_udcs();
	int len = count_strings(list);

	assert_int_equal(2, len);
	assert_string_equal("foreign", list[0]);
	assert_string_equal("descr", list[1]);

	free_string_array(list, len);
}

static int
foreign_cmd(const cmd_info_t *cmd_info)
{
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
