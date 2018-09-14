#include <stic.h>

#include "../../src/engine/cmds.h"
#include "../../src/cmd_core.h"

SETUP()
{
	init_commands();
}

TEARDOWN()
{
	vle_cmds_reset();
}

TEST(empty_command)
{
	assert_string_equal("", find_last_command(""));
	assert_string_equal("", find_last_command(":"));
	assert_string_equal("", find_last_command("::"));
}

TEST(single_command)
{
	assert_string_equal("quit", find_last_command("quit"));
}

TEST(commands_with_regex)
{
	assert_string_equal("filter a|b", find_last_command("filter a|b"));
	assert_string_equal("filter /a|b/", find_last_command("filter /a|b/"));
	assert_string_equal("hi /a|b/", find_last_command("hi /a|b/"));
	assert_string_equal("next", find_last_command("s/a|b/c/g|next"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
