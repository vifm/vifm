#include <stic.h>

#include <unistd.h> /* chdir() */

DEFINE_SUITE();

SETUP()
{
	assert_int_equal(0, chdir("test-data/sandbox"));
}

TEARDOWN()
{
	assert_int_equal(0, chdir("../.."));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
