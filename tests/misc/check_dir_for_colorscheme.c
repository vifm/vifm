#include "seatest.h"

#include "../../src/color_scheme.h"
#include "../../src/config.h"
#include "../../src/macros.h"

static void
setup(void)
{
	static Col_scheme my_schemes[] = {
		{.dir = "/"},
		{.dir = "/home"},
		{.dir = "/root"},
		{.dir = "/"},
	};
	col_schemes = (Col_scheme*)&my_schemes;
	
	cfg.color_scheme = 1;
	cfg.color_scheme_cur = 0;
	cfg.color_scheme_num = ARRAY_LEN(my_schemes);
}

static void
test_left_right(void)
{
	assert_int_equal(LCOLOR_BASE, check_directory_for_color_scheme(1, "/root"));
	assert_int_equal(RCOLOR_BASE, check_directory_for_color_scheme(0, "/root"));
}

void
check_dir_for_colorscheme_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);

	run_test(test_left_right);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
