#include <stdlib.h>

#include "seatest.h"

#include "../../src/filetype.h"
#include "../../src/status.h"

static void
test_one_ext(void)
{
	assoc_prog_t program;

	set_programs("*.tar", "tar prog", "description", 0);

	assert_true(get_default_program_for_file("file.version.tar", &program));
	if(program.description != NULL)
		assert_string_equal("description", program.description);
	free_assoc_prog(&program);
}

static void
test_two_exts(void)
{
	assoc_prog_t program;

	set_programs("*.tar,*.zip", "prog", "archives", 0);

	assert_true(get_default_program_for_file("file.version.tar", &program));
	if(program.description != NULL)
		assert_string_equal("archives", program.description);
	free_assoc_prog(&program);

	assert_true(get_default_program_for_file("file.version.zip", &program));
	if(program.description != NULL)
		assert_string_equal("archives", program.description);
	free_assoc_prog(&program);
}

void
description_tests(void)
{
	test_fixture_start();

	run_test(test_one_ext);
	run_test(test_two_exts);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
