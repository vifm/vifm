#include <string.h>

#include "seatest.h"

#include "../../src/sort.h"
#include "../../src/ui.h"

static void
setup(void)
{
	lwin.list_rows = 3;
	lwin.dir_entry = calloc(lwin.list_rows, sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("a");
	lwin.dir_entry[1].name = strdup("_");
	lwin.dir_entry[2].name = strdup("A");
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
test_special_chars_ignore_case_sort(void)
{
	int i;

	lwin.sort[0] = SORT_BY_INAME;
	for(i = 1; i < NUM_SORT_OPTIONS; i++)
		lwin.sort[i] = NUM_SORT_OPTIONS + 1;

	sort_view(&lwin);

	assert_string_equal("_", lwin.dir_entry[0].name);
}

void
sort_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_special_chars_ignore_case_sort);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
