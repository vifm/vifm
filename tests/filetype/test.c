#include <assert.h>

#include "seatest.h"

#include "../../src/filetype.h"
#include "../../src/status.h"

void filetype_tests(void);
void replace_double_comma_tests(void);
void description_tests(void);

static void
setup(void)
{
	curr_stats.is_console = 1;
}

static void
teardown(void)
{
	reset_all_file_associations();
}

static void
all_tests(void)
{
	filetype_tests();
	replace_double_comma_tests();
	description_tests();
}

int
main(int argc, char **argv)
{
	suite_setup(setup);
	suite_teardown(teardown);

	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
