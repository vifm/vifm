#include <stic.h>

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() */

#include "../../src/cfg/config.h"

TEST(entry_matching_input_is_skipped)
{
	assert_failure(cfg_set_fuse_home("relative/path"));
	assert_string_equal("", cfg.fuse_home);
	free(cfg.fuse_home);
	cfg.fuse_home = NULL;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
