#include <stic.h>

#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/cmd_core.h"

SETUP()
{
	init_commands();
	lwin.selected_files = 0;
	curr_view = &lwin;
}

TEST(wrong_command_name_causes_error)
{
	const char *file = TEST_DATA_PATH "/scripts/wrong-cmd-name.vifm";
	assert_failure(cfg_source_file(file));
}

TEST(wrong_user_defined_command_name_causes_error)
{
	const char *file = TEST_DATA_PATH "/scripts/wrong-udcmd-name.vifm";
	assert_failure(cfg_source_file(file));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
