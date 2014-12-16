#include <stdlib.h>
#include <string.h>

#include "seatest.h"

#include "../../src/filetype.h"
#include "../../src/status.h"
#include "../../src/utils/str.h"

static int
prog_exists(const char name[])
{
	return stroscmp(name, "console") == 0;
}

static void
test_find_program(void)
{
	assoc_record_t program;
	int success;

	ft_init(&prog_exists);

	ft_set_programs("*.tar.bz2", "no console prog", 0, 0);
	ft_set_programs("*.tar.bz2", "console prog", 0, 0);

	success = ft_get_program("file.version.tar.bz2", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("console prog", program.command);
		ft_assoc_record_free(&program);
	}
}

void
find_program_tests(void)
{
	test_fixture_start();

	run_test(test_find_program);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
