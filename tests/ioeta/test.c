#include "seatest.h"

void calculate_tests(void);
void lifetime_tests(void);
void update_tests(void);

void
all_tests(void)
{
	calculate_tests();
	lifetime_tests();
	update_tests();
}

int
main(void)
{
	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
