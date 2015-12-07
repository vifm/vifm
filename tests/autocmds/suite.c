#include <stic.h>

#include <stddef.h> /* NULL */

#include "../../src/engine/autocmds.h"

DEFINE_SUITE();

TEARDOWN()
{
	vle_aucmd_remove(NULL, NULL);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
