#include <stic.h>

#include <test-utils.h>

#include "../../src/cmd_completion.h"
#include "../../src/filetype.h"

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

TEST(exe_path_with_backslashes, IF(windows))
{
	ft_init(&external_command_exists);
	create_dir(SANDBOX_PATH "/dir");
	create_executable(SANDBOX_PATH "/dir/exe.exe");

	assert_true(ft_exists("\"" SANDBOX_PATH "\\dir\\exe.exe" "\""));

	remove_file(SANDBOX_PATH "/dir/exe.exe");
	remove_dir(SANDBOX_PATH "/dir");
	ft_init(NULL);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
