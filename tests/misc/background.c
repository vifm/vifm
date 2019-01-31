#include <stic.h>

#include "../../src/cfg/config.h"
#include "../../src/utils/cancellation.h"
#include "../../src/utils/str.h"
#include "../../src/background.h"

#include "utils.h"

TEST(background_redirects_streams_properly, IF(not_windows))
{
	update_string(&cfg.shell, "/bin/sh");
	update_string(&cfg.shell_cmd_flag, "-c");
	assert_success(bg_and_wait_for_errors("echo a", &no_cancellation));
	update_string(&cfg.shell, NULL);
	update_string(&cfg.shell_cmd_flag, NULL);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
