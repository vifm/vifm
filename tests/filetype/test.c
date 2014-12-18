#include <assert.h>

#include "seatest.h"

#include "../../src/filetype.h"
#include "../../src/status.h"

void filetype_tests(void);
void filextype_tests(void);
void classes_tests(void);
void replace_double_comma_tests(void);
void description_tests(void);
void find_program_tests(void);
void viewers_tests(void);

static void
setup(void)
{
	ft_init(NULL);
}

static void
teardown(void)
{
	ft_reset(0);
}

static void
all_tests(void)
{
	filetype_tests();
	filextype_tests();
	classes_tests();
	replace_double_comma_tests();
	description_tests();
	find_program_tests();
	viewers_tests();
}

int
main(int argc, char **argv)
{
	suite_setup(setup);
	suite_teardown(teardown);

	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
