#include <stdlib.h>
#include <string.h>

#include "seatest.h"

#include "../../src/filetype.h"
#include "../../src/status.h"
#include "../../src/utils/str.h"

static int prog_exists(const char *name)
{
	return stroscmp(name, "console") == 0;
}

static void
test_find_program(void)
{
	assoc_record_t program;
	int success;

	config_filetypes(&prog_exists);

	set_programs("*.tar.bz2", "no console prog", 0, 0);
	set_programs("*.tar.bz2", "console prog", 0, 0);

	success = get_default_program_for_file("file.version.tar.bz2", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("console prog", program.command);
		free_assoc_record(&program);
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
