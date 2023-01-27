#include <stic.h>

#include "../../src/engine/cmds.h"
#include "../../src/cmd_core.h"

SETUP()
{
	cmds_init();
}

TEARDOWN()
{
	vle_cmds_reset();
}

TEST(empty_command)
{
	assert_string_equal("", cmds_find_last(""));
	assert_string_equal("", cmds_find_last(":"));
	assert_string_equal("", cmds_find_last("::"));
}

TEST(single_command)
{
	assert_string_equal("quit", cmds_find_last("quit"));
}

TEST(commands_with_regex)
{
	assert_string_equal("filter a|b", cmds_find_last("filter a|b"));
	assert_string_equal("filter /a|b/", cmds_find_last("filter /a|b/"));
	assert_string_equal("hi /a|b/", cmds_find_last("hi /a|b/"));
	assert_string_equal("next", cmds_find_last("s/a|b/c/g|next"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
