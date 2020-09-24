#include <stic.h>

#include "../../src/engine/cmds.h"

TEST(can_reset_any_number_of_times)
{
	vle_cmds_reset();
	vle_cmds_reset();
	vle_cmds_reset();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
