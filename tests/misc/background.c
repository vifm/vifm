#include <stic.h>

#include "../../src/cfg/config.h"
#include "../../src/utils/str.h"
#include "../../src/background.h"

#include "utils.h"

TEST(background_redirects_streams_properly, IF(not_windows))
{
	update_string(&cfg.shell, "/bin/sh");
	assert_int_equal(0, background_and_wait_for_errors("echo a", 0));
	update_string(&cfg.shell, NULL);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
