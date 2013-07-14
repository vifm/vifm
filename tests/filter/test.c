#include "seatest.h"

#include "../../src/utils/filter.h"

void lifetime_tests(void);
void clear_tests(void);
void set_tests(void);
void append_tests(void);

static void
all_tests(void)
{
	lifetime_tests();
	clear_tests();
	set_tests();
	append_tests();
}

int
main(int argc, char **argv)
{
	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
