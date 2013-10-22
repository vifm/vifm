#include "seatest.h"

#include "../../src/commands_completion.h"

static void
test_system_shell_exists(void)
{
#ifdef _WIN32
	const char *const shell = "cmd";
#else
	const char *const shell = "sh";
#endif
	const int exists = external_command_exists(shell);
	assert_true(exists);
}

void
external_command_exists_tests(void)
{
	test_fixture_start();

	run_test(test_system_shell_exists);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
