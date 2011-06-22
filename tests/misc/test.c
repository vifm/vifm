#include "seatest.h"

void canonical(void);
void test_append_selected_files(void);
void test_expand_macros(void);

void all_tests(void)
{
	canonical();
	test_append_selected_files();
	test_expand_macros();
}

int main(int argc, char **argv)
{
	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
