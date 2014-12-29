#include <unistd.h> /* unlink() */

#include <stdlib.h> /* calloc() free() */
#include <string.h> /* strcpy() strdup() */

#include "seatest.h"

#include "../../src/utils/fs.h"
#include "../../src/utils/macros.h"
#include "../../src/fileops.h"

static void
setup(void)
{
	assert_int_equal(0, chdir("test-data/sandbox"));

	/* lwin */
	strcpy(lwin.curr_dir, ".");

	lwin.list_rows = 1;
	lwin.list_pos = 0;
	lwin.dir_entry = calloc(lwin.list_rows, sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("file");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];

	/* rwin */
	strcpy(rwin.curr_dir, ".");

	curr_view = &lwin;
	other_view = &rwin;
}

static void
teardown(void)
{
	int i;

	for(i = 0; i < lwin.list_rows; ++i)
	{
		free(lwin.dir_entry[i].name);
	}
	free(lwin.dir_entry);

	assert_int_equal(0, chdir("../.."));
}

static void
test_move_file(void)
{
	char new_fname[] = "new_name";
	char *list[] = { &new_fname[0] };

	FILE *const f = fopen(lwin.dir_entry[0].name, "w");
	fclose(f);

	assert_true(path_exists(lwin.dir_entry[0].name, DEREF));

	lwin.dir_entry[0].marked = 1;
	(void)cpmv_files(&lwin, list, ARRAY_LEN(list), CMLO_MOVE, 0);

	assert_false(path_exists(lwin.dir_entry[0].name, DEREF));
	assert_true(path_exists(new_fname, DEREF));

	(void)unlink(new_fname);
}

void
generic_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_move_file);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
