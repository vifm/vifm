#include <stic.h>

#include "../../src/engine/options.h"

extern int vifminfo;
extern int vifminfo_handler_calls;

TEST(assignment_to_something_calls_handler_only_once)
{
	int res;

	vifminfo = 0x00;

	vifminfo_handler_calls = 0;
	res = set_options("vifminfo=options,filetypes,commands,bookmarks");
	assert_int_equal(1, vifminfo_handler_calls);
	assert_int_equal(0, res);
	assert_int_equal(0x0f, vifminfo);
}

TEST(assignment_to_something)
{
	int res;

	vifminfo = 0x00;

	res = set_options("vifminfo=options,filetypes,commands,bookmarks");
	assert_int_equal(0, res);
	assert_int_equal(0x0f, vifminfo);
}

TEST(assignment_to_empty)
{
	int res;

	res = set_options("vifminfo=options,filetypes,commands,bookmarks");
	assert_int_equal(0, res);
	assert_int_equal(0x0f, vifminfo);

	res = set_options("vifminfo=");
	assert_int_equal(0, res);
	assert_int_equal(0x00, vifminfo);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
