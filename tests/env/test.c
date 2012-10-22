#include "seatest.h"

#include "../../src/utils/env.h"

void get_one_of_def_tests(void);

static void
all_tests(void)
{
	get_one_of_def_tests();
}

int
main(int argc, char *argv[])
{
	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
