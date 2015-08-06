#include <stic.h>

#include <stdlib.h>

#include "../../src/filetype.h"
#include "../../src/status.h"
#include "test.h"

TEST(regexp)
{
	const char *prog_cmd;

	set_programs("/.*\\.[ch]$/", "c file", 0, 0);

	assert_null(ft_get_program("main.cpp"));
	assert_null(ft_get_program("main.hpp"));

	assert_non_null(prog_cmd = ft_get_program("main.c"));
	assert_string_equal("c file", prog_cmd);

	assert_non_null(prog_cmd = ft_get_program("main.h"));
	assert_string_equal("c file", prog_cmd);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
