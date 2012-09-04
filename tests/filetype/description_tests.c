#include <stdlib.h>

#include "seatest.h"

#include "../../src/filetype.h"
#include "../../src/status.h"

static void
test_one_pattern(void)
{
	assoc_record_t program;
	int success;

	set_programs("*.tar", "{description} tar prog", 0, 0);

	success = get_default_program_for_file("file.version.tar", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("description", program.description);
		free_assoc_record(&program);
	}
}

static void
test_two_patterns(void)
{
	assoc_record_t program;
	int success;

	set_programs("*.tar,*.zip", "{archives} prog", 0, 0);

	success = get_default_program_for_file("file.version.tar", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("archives", program.description);
		free_assoc_record(&program);
	}

	success = get_default_program_for_file("file.version.zip", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("archives", program.description);
		free_assoc_record(&program);
	}
}

static void
test_two_programs(void)
{
	assoc_records_t ft;

	set_programs("*.tar", "{rar} rarprog, {zip} zipprog", 0, 0);

	ft = get_all_programs_for_file("a.tar");

	assert_int_equal(2, ft.count);

	assert_string_equal(ft.list[0].command, "rarprog");
	assert_string_equal(ft.list[0].description, "rar");

	assert_string_equal(ft.list[1].command, "zipprog");
	assert_string_equal(ft.list[1].description, "zip");

	free(ft.list);
}

void
description_tests(void)
{
	test_fixture_start();

	run_test(test_one_pattern);
	run_test(test_two_patterns);
	run_test(test_two_programs);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
