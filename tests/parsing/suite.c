#include <stic.h>

#include <stddef.h> /* NULL */

#include "../../src/engine/parsing.h"

#include <test-utils.h>

DEFINE_SUITE();

SETUP_ONCE()
{
	fix_environ();
}

SETUP()
{
	vle_parser_init(NULL);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
