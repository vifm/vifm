#include <assert.h>

#include "seatest.h"

#include "../../src/engine/completion.h"

void completion_tests(void);
void groups_unite_tests(void);

static void
setup(void)
{
}

static void
teardown(void)
{
	reset_completion();
}

static void
all_tests(void)
{
	completion_tests();
	groups_unite_tests();
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
