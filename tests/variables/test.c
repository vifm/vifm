#include <stdlib.h>

#include "seatest.h"

#include "../../src/engine/variables.h"
#include "../../src/utils/utils.h"

void format_tests(void);
void clear_tests(void);
void envvars_tests(void);
void completion_tests(void);

void
all_tests(void)
{
	format_tests();
	clear_tests();
	envvars_tests();
	completion_tests();
}

static void
setup(void)
{
	env_set("VAR_A", "VAL_A");
	env_set("VAR_B", "VAL_B");
	env_set("VAR_C", "VAL_C");

	init_variables();
}

static void
teardown(void)
{
	clear_variables();
	reset_completion();
}

int
main(int argc, char **argv)
{
	suite_setup(setup);
	suite_teardown(teardown);

	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
