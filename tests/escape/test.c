#include "seatest.h"

#include "../../src/utils/env.h"

void esc_highlight_pattern_tests(void);
void esc_remove_tests(void);
void esc_str_overhead_tests(void);

static void
all_tests(void)
{
	esc_highlight_pattern_tests();
	esc_remove_tests();
	esc_str_overhead_tests();
}

int
main(int argc, char *argv[])
{
	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
