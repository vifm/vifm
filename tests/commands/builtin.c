#include <stic.h>

#include <stddef.h> /* NULL */
#include <stdlib.h>
#include <string.h>

#include "../../src/engine/cmds.h"
#include "../../src/engine/completion.h"
#include "../../src/commands.h"

extern struct cmds_conf cmds_conf;

SETUP()
{
	reset_cmds();
	init_commands();
}

TEST(empty_line_completion)
{
	char *buf;

	vle_compl_reset();
	assert_int_equal(0, complete_cmd("", NULL));

	buf = vle_compl_next();
	assert_string_equal("!", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("alink", buf);
	free(buf);
}

TEST(set)
{
	char *buf;

	vle_compl_reset();
	assert_int_equal(0, complete_cmd("se", NULL));

	buf = vle_compl_next();
	assert_string_equal("set", buf);
	free(buf);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
