#include <stdlib.h>
#include <string.h>

#include "seatest.h"

#include "../../src/cfg/config.h"
#include "../../src/filelist.h"
#include "../../src/ui.h"

#define assert_hidden(view, name, dir) \
	assert_false(file_is_visible(&view, name, dir))

#define assert_visible(view, name, dir) \
	assert_true(file_is_visible(&view, name, dir))

#ifdef _WIN32
#define CASE_SENSATIVE_FILTER 0
#else
#define CASE_SENSATIVE_FILTER 1
#endif

static void
setup(void)
{
	cfg.filter_inverted_by_default = 1;

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

	filter_init(&lwin.name_filter, CASE_SENSATIVE_FILTER);
	filter_init(&lwin.auto_filter, CASE_SENSATIVE_FILTER);
	lwin.invert = cfg.filter_inverted_by_default;

	lwin.column_count = 1;

	rwin.list_rows = 8;
	rwin.list_pos = 2;
	rwin.dir_entry = calloc(rwin.list_rows, sizeof(*rwin.dir_entry));
	rwin.dir_entry[0].name = strdup("dir1.d");
	rwin.dir_entry[1].name = strdup("dir2.d");
	rwin.dir_entry[2].name = strdup("dir3.d");
	rwin.dir_entry[3].name = strdup("file1.d");
	rwin.dir_entry[4].name = strdup("file2.d");
	rwin.dir_entry[5].name = strdup("file3.d");
	rwin.dir_entry[6].name = strdup("withnonodots");
	rwin.dir_entry[7].name = strdup("withnonodots/");

	rwin.dir_entry[0].selected = 0;
	rwin.dir_entry[1].selected = 0;
	rwin.dir_entry[2].selected = 0;
	rwin.dir_entry[3].selected = 0;
	rwin.dir_entry[4].selected = 0;
	rwin.dir_entry[5].selected = 0;
	rwin.dir_entry[6].selected = 0;
	rwin.selected_files = 0;

	filter_init(&rwin.name_filter, CASE_SENSATIVE_FILTER);
	filter_init(&rwin.auto_filter, CASE_SENSATIVE_FILTER);
	rwin.invert = cfg.filter_inverted_by_default;

	rwin.column_count = 1;
}

static void cleanup_view(FileView *view)
{
	int i;

	for(i = 0; i < view->list_rows; i++)
		free(view->dir_entry[i].name);
	free(view->dir_entry);
	filter_dispose(&view->name_filter);
	filter_dispose(&view->auto_filter);
}

static void
teardown(void)
{
	cleanup_view(&lwin);
	cleanup_view(&rwin);
}

static void
test_filtering(void)
{
	int i;

	filter_selected_files(&lwin);

	for(i = 0; i < lwin.list_rows - 1; i++)
		assert_hidden(lwin, lwin.dir_entry[i].name, 0);
	assert_visible(lwin, lwin.dir_entry[i].name, 0);
}

static void
test_filtering_file_does_not_filter_dir(void)
{
	rwin.dir_entry[6].selected = 1;
	rwin.selected_files = 1;

	filter_selected_files(&rwin);

	assert_hidden(rwin, rwin.dir_entry[6].name, 0);
	/* Pass 0 as if for file, because we already have trailing slash there. */
	assert_visible(rwin, rwin.dir_entry[7].name, 0);
}

static void
test_filtering_dir_does_not_filter_file(void)
{
	rwin.dir_entry[7].selected = 1;
	rwin.selected_files = 1;

	filter_selected_files(&rwin);

	/* Pass 0 as if for file, because we already have trailing slash there. */
	assert_hidden(rwin, rwin.dir_entry[7].name, 0);
	assert_visible(rwin, rwin.dir_entry[6].name, 0);
}

static void
test_filtering_files_does_not_filter_dirs(void)
{
	(void)filter_set(&rwin.name_filter, "^.*\\.d$");

	assert_visible(rwin, rwin.dir_entry[0].name, 1);
	assert_visible(rwin, rwin.dir_entry[1].name, 1);
	assert_visible(rwin, rwin.dir_entry[2].name, 1);
	assert_hidden(rwin, rwin.dir_entry[3].name, 0);
	assert_hidden(rwin, rwin.dir_entry[4].name, 0);
	assert_hidden(rwin, rwin.dir_entry[5].name, 0);
}

static void
test_filtering_dirs_does_not_filter_files(void)
{
	(void)filter_set(&rwin.name_filter, "^.*\\.d/$");

	assert_hidden(rwin, rwin.dir_entry[0].name, 1);
	assert_hidden(rwin, rwin.dir_entry[1].name, 1);
	assert_hidden(rwin, rwin.dir_entry[2].name, 1);
	assert_visible(rwin, rwin.dir_entry[3].name, 0);
	assert_visible(rwin, rwin.dir_entry[4].name, 0);
	assert_visible(rwin, rwin.dir_entry[5].name, 0);
}

static void
test_filtering_files_and_dirs(void)
{
	(void)filter_set(&rwin.name_filter, "^.*\\.d/?$");

	assert_hidden(rwin, rwin.dir_entry[0].name, 1);
	assert_hidden(rwin, rwin.dir_entry[1].name, 1);
	assert_hidden(rwin, rwin.dir_entry[2].name, 1);
	assert_hidden(rwin, rwin.dir_entry[3].name, 0);
	assert_hidden(rwin, rwin.dir_entry[4].name, 0);
	assert_hidden(rwin, rwin.dir_entry[5].name, 0);
}

void
filtering_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_filtering);
	run_test(test_filtering_file_does_not_filter_dir);
	run_test(test_filtering_dir_does_not_filter_file);
	run_test(test_filtering_files_does_not_filter_dirs);
	run_test(test_filtering_dirs_does_not_filter_files);
	run_test(test_filtering_files_and_dirs);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
