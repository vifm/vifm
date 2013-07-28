#include <string.h>

#include "seatest.h"

#include "../../src/engine/cmds.h"
#include "../../src/engine/completion.h"
#include "../../src/commands.h"

#include "asserts.h"

static void
vim_like_completion(void)
{
	init_commands();

	reset_completion();
	assert_int_equal(0, complete_cmd("e"));
	ASSERT_NEXT_MATCH("echo");
	ASSERT_NEXT_MATCH("edit");
	ASSERT_NEXT_MATCH("else");
	ASSERT_NEXT_MATCH("empty");
	ASSERT_NEXT_MATCH("endif");
	ASSERT_NEXT_MATCH("execute");
	ASSERT_NEXT_MATCH("exit");
	ASSERT_NEXT_MATCH("e");

	reset_completion();
	assert_int_equal(0, complete_cmd("vm"));
	ASSERT_NEXT_MATCH("vmap");
	ASSERT_NEXT_MATCH("vmap");

	reset_completion();
	assert_int_equal(0, complete_cmd("j"));
	ASSERT_NEXT_MATCH("jobs");
	ASSERT_NEXT_MATCH("jobs");

	reset_cmds();
}

void
test_reserved_commands(void)
{
	test_fixture_start();

	run_test(vim_like_completion);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
