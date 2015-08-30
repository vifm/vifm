#include <stic.h>

#include <stddef.h> /* NULL */
#include <string.h> /* strcmp() */

#include "../../src/ui/ui.h"
#include "../../src/commands.h"

SETUP()
{
	init_commands();
	lwin.selected_files = 0;
}

TEST(tag_with_comma_is_rejected)
{
	assert_failure(exec_commands("bmark a,b", &lwin, CIT_COMMAND));
}

TEST(tag_with_space_is_rejected)
{
	assert_failure(exec_commands("bmark a\\ b", &lwin, CIT_COMMAND));
}

TEST(emark_allows_specifying_bookmark_path)
{
	assert_success(exec_commands("bmark! /fake/path tag", &lwin, CIT_COMMAND));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
