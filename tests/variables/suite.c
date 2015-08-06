#include <stic.h>

#include <stdlib.h>

#include "../../src/engine/completion.h"
#include "../../src/engine/variables.h"
#include "../../src/utils/env.h"
#include "../../src/utils/utils.h"

DEFINE_SUITE();

SETUP()
{
	env_set("VAR_A", "VAL_A");
	env_set("VAR_B", "VAL_B");
	env_set("VAR_C", "VAL_C");

	init_variables();
}

TEARDOWN()
{
	clear_variables();
	vle_compl_reset();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
