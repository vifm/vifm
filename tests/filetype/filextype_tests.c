#include <stdlib.h>

#include "seatest.h"

#include "../../src/filetype.h"
#include "../../src/status.h"

static void
test_1_c(void)
{
	const char *prog_cmd;

	ft_set_programs("*.tar", "x prog", 1, 0);
	ft_set_programs("*.tar", "console prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("file.version.tar")) != NULL);
	assert_string_equal("console prog", prog_cmd);
}

static void
test_1_x(void)
{
	const char *prog_cmd;

	ft_set_programs("*.tar", "x prog", 1, 1);
	ft_set_programs("*.tar", "console prog", 0, 1);

	assert_true((prog_cmd = ft_get_program("file.version.tar")) != NULL);
	assert_string_equal("x prog", prog_cmd);
}

static void
test_2_c(void)
{
	const char *prog_cmd;

	ft_set_programs("*.tgz", "2 x prog", 1, 0);

	assert_true((prog_cmd = ft_get_program("file.version.tgz")) == NULL);
}

static void
test_2_x(void)
{
	const char *prog_cmd;

	ft_set_programs("*.tgz", "2 x prog", 1, 1);

	assert_true((prog_cmd = ft_get_program("file.version.tgz")) != NULL);
	assert_string_equal("2 x prog", prog_cmd);
}

static void
test_3_c(void)
{
	const char *prog_cmd;

	ft_set_programs("*.tar.bz2", "3 console prog", 0, 1);

	assert_true((prog_cmd = ft_get_program("file.version.tar.bz2")) != NULL);
	assert_string_equal("3 console prog", prog_cmd);
}

static void
test_3_x(void)
{
	const char *prog_cmd;

	ft_set_programs("*.tar.bz2", "3 console prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("file.version.tar.bz2")) != NULL);
	assert_string_equal("3 console prog", prog_cmd);
}

void
filextype_tests(void)
{
	test_fixture_start();

	run_test(test_1_c);
	run_test(test_1_x);
	run_test(test_2_c);
	run_test(test_2_x);
	run_test(test_3_c);
	run_test(test_3_x);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
