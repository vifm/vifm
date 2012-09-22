#include "seatest.h"

#include "../../src/engine/parsing.h"

void general_tests(void);
void single_quoted_tests(void);
void double_quoted_tests(void);
void envvar_tests(void);
void functions_tests(void);
void statements_tests(void);

void
all_tests(void)
{
	general_tests();
	single_quoted_tests();
	double_quoted_tests();
	envvar_tests();
	functions_tests();
	statements_tests();
}

int
main(void)
{
	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
