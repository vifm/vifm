#include <stic.h>

#include "../../src/cmd_core.h"

TEST(test_whole_line_comments)
{
	assert_int_equal(0, exec_command("\"", NULL, CIT_COMMAND));
	assert_int_equal(0, exec_command(" \"", NULL, CIT_COMMAND));
	assert_int_equal(0, exec_command("  \"", NULL, CIT_COMMAND));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
