#include <string.h>

#include "seatest.h"

#include "../../src/commands.h"

static void
should_be_sorted(void)
{
	int i;

	for(i = 0; i < RESERVED - 1; i++)
	{
		assert_true(strcmp(reserved_commands[i], reserved_commands[i + 1]) < 0);
	}
}

void
test_reserved_commands(void)
{
	test_fixture_start();

	run_test(should_be_sorted);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
