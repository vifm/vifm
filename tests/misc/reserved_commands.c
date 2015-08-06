#include <stic.h>

#include <stddef.h> /* NULL */
#include <string.h>

#include "../../src/engine/cmds.h"
#include "../../src/engine/completion.h"
#include "../../src/commands.h"

#include "asserts.h"

TEST(vim_like_completion)
{
	init_commands();

	vle_compl_reset();
	assert_int_equal(0, complete_cmd("e", NULL));
	ASSERT_NEXT_MATCH("echo");
	ASSERT_NEXT_MATCH("edit");
	ASSERT_NEXT_MATCH("else");
	ASSERT_NEXT_MATCH("empty");
	ASSERT_NEXT_MATCH("endif");
	ASSERT_NEXT_MATCH("execute");
	ASSERT_NEXT_MATCH("exit");
	ASSERT_NEXT_MATCH("e");

	vle_compl_reset();
	assert_int_equal(0, complete_cmd("vm", NULL));
	ASSERT_NEXT_MATCH("vmap");
	ASSERT_NEXT_MATCH("vmap");

	vle_compl_reset();
	assert_int_equal(0, complete_cmd("j", NULL));
	ASSERT_NEXT_MATCH("jobs");
	ASSERT_NEXT_MATCH("jobs");

	reset_cmds();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
