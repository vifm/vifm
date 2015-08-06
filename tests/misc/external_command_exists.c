#include <stic.h>

#include "../../src/commands_completion.h"

TEST(system_shell_exists)
{
#ifdef _WIN32
	const char *const shell = "cmd";
#else
	const char *const shell = "sh";
#endif
	const int exists = external_command_exists(shell);
	assert_true(exists);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
