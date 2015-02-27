#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include <unistd.h> /* chdir() */

#include "../../src/cfg/config.h"

DEFINE_SUITE();

SETUP()
{
	cfg.shell = strdup("/bin/bash");

	assert_int_equal(0, chdir("test-data/sandbox"));
}

TEARDOWN()
{
	assert_int_equal(0, chdir("../.."));

	free(cfg.shell);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
