#include "seatest.h"

void one_number_range(void);
void user_command_and_exclamation_mark(void);
void built_in(void);

void all_tests(void)
{
	one_number_range();
	user_command_and_exclamation_mark();
	built_in();
}

int main(int argc, char **argv)
{
	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
