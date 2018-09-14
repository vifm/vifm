#include <stic.h>

#include <stddef.h> /* NULL */
#include <stdlib.h>
#include <string.h>

#include "../../src/engine/cmds.h"
#include "../../src/engine/completion.h"
#include "../../src/cmd_core.h"

extern struct cmds_conf cmds_conf;

SETUP()
{
	vle_cmds_reset();
	init_commands();
}

TEST(empty_line_completion)
{
	char *buf;

	vle_compl_reset();
	assert_int_equal(0, vle_cmds_complete("", NULL));

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
	assert_int_equal(0, vle_cmds_complete("se", NULL));

	buf = vle_compl_next();
	assert_string_equal("select", buf);
	free(buf);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
