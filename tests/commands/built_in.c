#include <string.h>

#include "seatest.h"

#include "../../src/config.h"
#include "../../src/commands.h"

static void
test_set(void)
{
	char *buf;

	buf = command_completion("se", 0);
	assert_string_equal("set", buf);
	free(buf);
}

void
built_in(void)
{
	test_fixture_start();

	run_test(test_set);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
