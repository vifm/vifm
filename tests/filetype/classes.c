#include <stic.h>

#include <stdlib.h>

#include "../../src/filetype.h"
#include "../../src/status.h"

TEST(enumeration)
{
	const char *prog_cmd;

	ft_set_programs("*.[ch]", 1, "c file", 0, 0);

	assert_true((prog_cmd = ft_get_program("main.cpp")) == NULL);
	assert_true((prog_cmd = ft_get_program("main.hpp")) == NULL);

	assert_true((prog_cmd = ft_get_program("main.c")) != NULL);
	assert_string_equal("c file", prog_cmd);

	assert_true((prog_cmd = ft_get_program("main.h")) != NULL);
	assert_string_equal("c file", prog_cmd);
}

TEST(negotiation_with_emark)
{
	const char *prog_cmd;

	ft_set_programs("*.[!ch]", 1, "not c file", 0, 0);

	assert_false((prog_cmd = ft_get_program("main.c")) != NULL);
	assert_false((prog_cmd = ft_get_program("main.h")) != NULL);

	assert_true((prog_cmd = ft_get_program("main.o")) != NULL);
	assert_string_equal("not c file", prog_cmd);
}

TEST(negotiation_with_hat)
{
	const char *prog_cmd;

	ft_set_programs("*.[^ch]", 1, "not c file", 0, 0);

	assert_false((prog_cmd = ft_get_program("main.c")) != NULL);
	assert_false((prog_cmd = ft_get_program("main.h")) != NULL);

	assert_true((prog_cmd = ft_get_program("main.o")) != NULL);
	assert_string_equal("not c file", prog_cmd);
}

TEST(ranges)
{
	const char *prog_cmd;

	ft_set_programs("*.[0-9]", 1, "part file", 0, 0);

	assert_false((prog_cmd = ft_get_program("main.A")) != NULL);

	assert_true((prog_cmd = ft_get_program("main.0")) != NULL);
	assert_string_equal("part file", prog_cmd);

	assert_true((prog_cmd = ft_get_program("main.8")) != NULL);
	assert_string_equal("part file", prog_cmd);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
