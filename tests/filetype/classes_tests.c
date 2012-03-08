#include <stdlib.h>

#include "seatest.h"

#include "../../src/filetype.h"
#include "../../src/status.h"

static void
test_enumeration(void)
{
	assoc_record_t program;

	set_programs("*.[ch]", "c file", 0);

	assert_false(get_default_program_for_file("main.cpp", &program));
	assert_false(get_default_program_for_file("main.hpp", &program));

	assert_true(get_default_program_for_file("main.c", &program));
	if(program.command != NULL)
		assert_string_equal("c file", program.command);
	free_assoc_record(&program);

	assert_true(get_default_program_for_file("main.h", &program));
	if(program.command != NULL)
		assert_string_equal("c file", program.command);
	free_assoc_record(&program);
}

static void
test_negotiation_with_emark(void)
{
	assoc_record_t program;

	set_programs("*.[!ch]", "not c file", 0);

	assert_false(get_default_program_for_file("main.c", &program));
	assert_false(get_default_program_for_file("main.h", &program));

	assert_true(get_default_program_for_file("main.o", &program));
	if(program.command != NULL)
		assert_string_equal("not c file", program.command);
	free_assoc_record(&program);
}

static void
test_negotiation_with_hat(void)
{
	assoc_record_t program;

	set_programs("*.[^ch]", "not c file", 0);

	assert_false(get_default_program_for_file("main.c", &program));
	assert_false(get_default_program_for_file("main.h", &program));

	assert_true(get_default_program_for_file("main.o", &program));
	if(program.command != NULL)
		assert_string_equal("not c file", program.command);
	free_assoc_record(&program);
}

static void
test_ranges(void)
{
	assoc_record_t program;

	set_programs("*.[0-9]", "part file", 0);

	assert_false(get_default_program_for_file("main.c", &program));
	assert_false(get_default_program_for_file("main.A", &program));

	assert_true(get_default_program_for_file("main.0", &program));
	if(program.command != NULL)
		assert_string_equal("part file", program.command);
	free_assoc_record(&program);

	assert_true(get_default_program_for_file("main.8", &program));
	if(program.command != NULL)
		assert_string_equal("part file", program.command);
	free_assoc_record(&program);
}

void
classes_tests(void)
{
	test_fixture_start();

	run_test(test_enumeration);
	run_test(test_negotiation_with_emark);
	run_test(test_negotiation_with_hat);
	run_test(test_ranges);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
