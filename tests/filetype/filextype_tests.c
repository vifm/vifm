#include <stdlib.h>

#include "seatest.h"

#include "../../src/filetype.h"
#include "../../src/status.h"

static void
test_1_c(void)
{
	assoc_prog_t program;

	curr_stats.is_console = 1;
	set_programs("*.tar", "x prog", 1);
	set_programs("*.tar", "console prog", 0);

	assert_true(get_default_program_for_file("file.version.tar", &program));
	if(program.com != NULL)
		assert_string_equal("console prog", program.com);
	free_assoc_prog(&program);
}

static void
test_1_x(void)
{
	assoc_prog_t program;

	curr_stats.is_console = 0;
	set_programs("*.tar", "x prog", 1);
	set_programs("*.tar", "console prog", 0);

	assert_true(get_default_program_for_file("file.version.tar", &program));
	if(program.com != NULL)
		assert_string_equal("x prog", program.com);
	free_assoc_prog(&program);
}

static void
test_2_c(void)
{
	assoc_prog_t program;

	curr_stats.is_console = 1;
	set_programs("*.tgz", "2 x prog", 1);

	assert_false(get_default_program_for_file("file.version.tgz", &program));
}

static void
test_2_x(void)
{
	assoc_prog_t program;

	curr_stats.is_console = 0;
	set_programs("*.tgz", "2 x prog", 1);

	assert_true(get_default_program_for_file("file.version.tgz", &program));
	if(program.com != NULL)
		assert_string_equal("2 x prog", program.com);
	free_assoc_prog(&program);
}

static void
test_3_c(void)
{
	assoc_prog_t program;

	curr_stats.is_console = 0;
	set_programs("*.tar.bz2", "3 console prog", 0);

	assert_true(get_default_program_for_file("file.version.tar.bz2", &program));
	if(program.com != NULL)
		assert_string_equal("3 console prog", program.com);
	free_assoc_prog(&program);
}

static void
test_3_x(void)
{
	assoc_prog_t program;

	curr_stats.is_console = 1;
	set_programs("*.tar.bz2", "3 console prog", 0);

	assert_true(get_default_program_for_file("file.version.tar.bz2", &program));
	if(program.com != NULL)
		assert_string_equal("3 console prog", program.com);
	free_assoc_prog(&program);
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
