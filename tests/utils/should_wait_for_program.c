#include <stic.h>

#include "../../src/utils/fs.h"
#include "../../src/utils/utils.h"

#ifdef _WIN32

static int cmd_exists(void);
static int mspaint_exists(void);

TEST(console_program_is_detected, IF(cmd_exists))
{
	assert_true(should_wait_for_program("c:/windows/system32/cmd.exe"));
}

TEST(graphical_program_is_detected, IF(mspaint_exists))
{
	assert_false(should_wait_for_program("c:/windows/system32/mspaint.exe"));
}

TEST(backslashes_do_not_affect_outcome_console, IF(cmd_exists))
{
	assert_true(should_wait_for_program("c:\\windows\\system32\\cmd.exe"));
}

TEST(backslashes_do_not_affect_outcome_graphical, IF(mspaint_exists))
{
	assert_false(should_wait_for_program("c:\\windows\\system32\\mspaint.exe"));
}

static int
cmd_exists(void)
{
	return path_exists("c:/windows/system32/cmd.exe", DEREF);
}

static int
mspaint_exists(void)
{
	return path_exists("c:/windows/system32/mspaint.exe", DEREF);
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
