#include "seatest.h"

void one_number_range(void);

void all_tests(void)
{
	one_number_range();
}

void my_suite_setup(void)
{
}

void my_suite_teardown(void)
{
}

int main(int argc, char **argv)
{
	suite_setup(my_suite_setup);
	suite_teardown(my_suite_teardown);

	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
