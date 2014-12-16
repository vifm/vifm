#include <stdlib.h>

#include "seatest.h"

#include "../../src/filetype.h"
#include "../../src/status.h"

static void
test_enumeration(void)
{
	assoc_record_t program;
	int success;

	ft_set_programs("*.[ch]", "c file", 0, 0);

	assert_false(ft_get_program("main.cpp", &program));
	assert_false(ft_get_program("main.hpp", &program));

	success = ft_get_program("main.c", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("c file", program.command);
		ft_assoc_record_free(&program);
	}

	success = ft_get_program("main.h", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("c file", program.command);
		ft_assoc_record_free(&program);
	}
}

static void
test_negotiation_with_emark(void)
{
	assoc_record_t program;
	int success;

	ft_set_programs("*.[!ch]", "not c file", 0, 0);

	assert_false(ft_get_program("main.c", &program));
	assert_false(ft_get_program("main.h", &program));

	success = ft_get_program("main.o", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("not c file", program.command);
		ft_assoc_record_free(&program);
	}
}

static void
test_negotiation_with_hat(void)
{
	assoc_record_t program;
	int success;

	ft_set_programs("*.[^ch]", "not c file", 0, 0);

	assert_false(ft_get_program("main.c", &program));
	assert_false(ft_get_program("main.h", &program));

	success = ft_get_program("main.o", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("not c file", program.command);
		ft_assoc_record_free(&program);
	}
}

static void
test_ranges(void)
{
	assoc_record_t program;
	int success;

	ft_set_programs("*.[0-9]", "part file", 0, 0);

	assert_false(ft_get_program("main.A", &program));

	success = ft_get_program("main.0", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("part file", program.command);
		ft_assoc_record_free(&program);
	}

	success = ft_get_program("main.8", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("part file", program.command);
		ft_assoc_record_free(&program);
	}
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
