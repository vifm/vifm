#include <stic.h>

#include "../../src/engine/options.h"

extern int vifminfo;
extern int vifminfo_handler_calls;

TEST(assignment_to_something_calls_handler_only_once)
{
	int res;

	vifminfo = 0x00;

	vifminfo_handler_calls = 0;
	res = vle_opts_set("vifminfo=options,filetypes,commands,bookmarks",
			OPT_GLOBAL);
	assert_int_equal(1, vifminfo_handler_calls);
	assert_int_equal(0, res);
	assert_int_equal(0x0f, vifminfo);
}

TEST(assignment_to_something)
{
	int res;

	vifminfo = 0x00;

	res = vle_opts_set("vifminfo=options,filetypes,commands,bookmarks",
			OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_int_equal(0x0f, vifminfo);
}

TEST(assignment_to_empty)
{
	int res;

	res = vle_opts_set("vifminfo=options,filetypes,commands,bookmarks",
			OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_int_equal(0x0f, vifminfo);

	res = vle_opts_set("vifminfo=", OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_int_equal(0x00, vifminfo);
}

TEST(huge_set_value)
{
	assert_success(vle_opts_set("vifminfo=optionsfiletypescommandsbookmarksoption"
				"filetypescommandsbooptionsfiletypescommandsbookmarksoptionsfiletypesco"
				"mmandsbooptionsfiletypescommandsbookmarksoptionsfiletypescommandsboopt"
				"ionsfiletypescommandsbookmarksoptionsfiletypescommandsbookmarksoptions"
				"filetypescommandsbookmarksoptionsfiletypescommandsbookmarksoptionsfile"
				"typescommandsbookmarksoptionsfiletypescommandsbookmarksoptionsfiletype"
				"scommandsbookmarksoptionsfiletypescommandsbookmarksoptionsfiletypescom"
				"mandsbookmarksoptionsfiletypescommandsbookmarksoptionsfiletypescommand"
				"sbookmarksoptionsfiletypescommandsbookmarksoptionsfiletypescommandsboo"
				"kmarksoptionsfiletypescommandsbookmarksoptionsfiletypescommandsbookmar"
				"ksoptionsfiletypescommandsbookmarksoptionsfiletypescommandsbookmarksop"
				"tionsfiletypescommandsbookmarksoptionsfiletypescommandsbookmarksoption"
				"sfiletypescommandsbookmarksoptionsfiletypescommandsbookmarksoptionsfil"
				"etypescommandsbookmarksoptionsfiletypescommandsbookmarksoptionsfiletyp"
				"escommandsbookmarksoptionsfiletypescommandsbookmarksoptionsfiletypesco"
				"mmandsbookmarksoptionsfiletypescommandsbookmarksoptionsfiletypescomman"
				"dsbookmarksoptionsfiletypescommandsbookmarkss", OPT_GLOBAL));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
