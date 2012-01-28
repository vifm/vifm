#include <stdlib.h>
#include <string.h>

#include "seatest.h"

#include "../../src/filelist.h"
#include "../../src/ui.h"

static void
setup(void)
{
	lwin.list_rows = 7;
	lwin.list_pos = 2;
	lwin.dir_entry = calloc(lwin.list_rows, sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("with(round)");
	lwin.dir_entry[1].name = strdup("with[square]");
	lwin.dir_entry[2].name = strdup("with{curly}");
	lwin.dir_entry[3].name = strdup("with<angle>");
	lwin.dir_entry[4].name = strdup("withSPECS+*^$?|\\");
	lwin.dir_entry[5].name = strdup("with....dots");
	lwin.dir_entry[6].name = strdup("withnonodots");

	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].selected = 1;
	lwin.dir_entry[2].selected = 1;
	lwin.dir_entry[3].selected = 1;
	lwin.dir_entry[4].selected = 1;
	lwin.dir_entry[5].selected = 1;
	lwin.dir_entry[6].selected = 0;
	lwin.selected_files = 6;

	lwin.filename_filter = strdup("");
	lwin.invert = 0;
}

static void
teardown(void)
{
	int i;

	for(i = 0; i < lwin.list_rows; i++)
		free(lwin.dir_entry[i].name);
	free(lwin.dir_entry);
}

static void
test_filtering(void)
{
	int i;

	filter_selected_files(&lwin);

	for(i = 0; i < lwin.list_rows - 1; i++)
		assert_true(regexp_filter_match(&lwin, lwin.dir_entry[i].name) == 0);
	assert_false(regexp_filter_match(&lwin, lwin.dir_entry[i].name) == 0);
}

void
filtering_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_filtering);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
