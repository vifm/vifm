#include <stdlib.h>

#include "seatest.h"

#include "../../src/filetype.h"
#include "../../src/status.h"

static void
test_enumeration(void)
{
	const char *prog_cmd;

	ft_set_programs("*.[ch]", "c file", 0, 0);

	assert_true((prog_cmd = ft_get_program("main.cpp")) == NULL);
	assert_true((prog_cmd = ft_get_program("main.hpp")) == NULL);

	assert_true((prog_cmd = ft_get_program("main.c")) != NULL);
	assert_string_equal("c file", prog_cmd);

	assert_true((prog_cmd = ft_get_program("main.h")) != NULL);
	assert_string_equal("c file", prog_cmd);
}

static void
test_negotiation_with_emark(void)
{
	const char *prog_cmd;

	ft_set_programs("*.[!ch]", "not c file", 0, 0);

	assert_false((prog_cmd = ft_get_program("main.c")) != NULL);
	assert_false((prog_cmd = ft_get_program("main.h")) != NULL);

	assert_true((prog_cmd = ft_get_program("main.o")) != NULL);
	assert_string_equal("not c file", prog_cmd);
}

static void
test_negotiation_with_hat(void)
{
	const char *prog_cmd;

	ft_set_programs("*.[^ch]", "not c file", 0, 0);

	assert_false((prog_cmd = ft_get_program("main.c")) != NULL);
	assert_false((prog_cmd = ft_get_program("main.h")) != NULL);

	assert_true((prog_cmd = ft_get_program("main.o")) != NULL);
	assert_string_equal("not c file", prog_cmd);
}

static void
test_ranges(void)
{
	const char *prog_cmd;

	ft_set_programs("*.[0-9]", "part file", 0, 0);

	assert_false((prog_cmd = ft_get_program("main.A")) != NULL);

	assert_true((prog_cmd = ft_get_program("main.0")) != NULL);
	assert_string_equal("part file", prog_cmd);

	assert_true((prog_cmd = ft_get_program("main.8")) != NULL);
	assert_string_equal("part file", prog_cmd);
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
