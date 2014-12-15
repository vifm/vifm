#include <stdlib.h>
#include <string.h>

#include "seatest.h"

#include "../../src/filetype.h"
#include "../../src/status.h"
#include "../../src/utils/str.h"

static int
prog1_available(const char name[])
{
	return stroscmp(name, "prog1") == 0;
}

static int
prog2_available(const char name[])
{
	return stroscmp(name, "prog2") == 0;
}

static int
nothing_available(const char name[])
{
	return 0;
}

static void
test_null_if_nothing_set(void)
{
	assert_true(get_viewer_for_file("file.version.tar.bz2") == NULL);
}

static void
test_multiple_choice_separated(void)
{
	const char *viewer;

	set_fileviewers("*.tar.bz2", "prog1");
	set_fileviewers("*.tar.bz2", "prog2");

	config_filetypes(&prog1_available);
	viewer = get_viewer_for_file("file.version.tar.bz2");
	assert_true(viewer != NULL);
	if(viewer != NULL)
	{
		assert_string_equal("prog1", viewer);
	}

	config_filetypes(&prog2_available);
	viewer = get_viewer_for_file("file.version.tar.bz2");
	assert_true(viewer != NULL);
	if(viewer != NULL)
	{
		assert_string_equal("prog2", viewer);
	}

	config_filetypes(&nothing_available);
	viewer = get_viewer_for_file("file.version.tar.bz2");
	assert_true(viewer == NULL);
}

static void
test_multiple_choice_joined(void)
{
	const char *viewer;

	set_fileviewers("*.tar.bz2", "prog1,prog2");

	config_filetypes(&prog1_available);
	viewer = get_viewer_for_file("file.version.tar.bz2");
	assert_true(viewer != NULL);
	if(viewer != NULL)
	{
		assert_string_equal("prog1", viewer);
	}

	config_filetypes(&prog2_available);
	viewer = get_viewer_for_file("file.version.tar.bz2");
	assert_true(viewer != NULL);
	if(viewer != NULL)
	{
		assert_string_equal("prog2", viewer);
	}

	config_filetypes(&nothing_available);
	viewer = get_viewer_for_file("file.version.tar.bz2");
	assert_true(viewer == NULL);
}

void
viewers_tests(void)
{
	test_fixture_start();

	run_test(test_null_if_nothing_set);
	run_test(test_multiple_choice_separated);
	run_test(test_multiple_choice_joined);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
