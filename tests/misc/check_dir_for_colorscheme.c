#include "seatest.h"

#include "../../src/color_scheme.h"
#include "../../src/config.h"
#include "../../src/macros.h"

static void
setup(void)
{
	Col_scheme my_schemes[] = {
		{.dir = "/"},
		{.dir = "/home"},
		{.dir = "/root"},
	};
	col_schemes = (Col_scheme*)&my_schemes;
	
	cfg.color_scheme_num = ARRAY_LEN(my_schemes);
}

static void
test_dirs(void)
{
	assert_int_equal(0, check_directory_for_color_scheme("/"));
	assert_int_equal(0, check_directory_for_color_scheme("/home_"));
	assert_int_equal(MAXNUM_COLOR, check_directory_for_color_scheme("/home"));
	assert_int_equal(MAXNUM_COLOR, check_directory_for_color_scheme("/home/"));
	assert_int_equal(MAXNUM_COLOR, check_directory_for_color_scheme("/home/a/"));
	assert_int_equal(0, check_directory_for_color_scheme("/roo"));
	assert_int_equal(MAXNUM_COLOR*2, check_directory_for_color_scheme("/root"));
}

void
check_dir_for_colorscheme_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);

	run_test(test_dirs);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
