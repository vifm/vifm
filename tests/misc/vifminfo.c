#include <stic.h>

#include <stdio.h> /* fclose() fopen() fprintf() remove() */

#include "../../src/cfg/config.h"
#include "../../src/cfg/info.h"
#include "../../src/cfg/info_chars.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"

#include "utils.h"

TEST(view_sorting_is_read_from_vifminfo)
{
	FILE *const f = fopen(SANDBOX_PATH "/vifminfo", "w");
	fprintf(f, "%c%d,%d", LINE_TYPE_LWIN_SORT, SK_BY_NAME, -SK_BY_DIR);
	fclose(f);

	/* ls-like view blocks view column updates. */
	lwin.ls_view = 1;
	copy_str(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH);
	read_info_file(1);
	lwin.ls_view = 0;

	opt_handlers_setup();

	copy_str(lwin.curr_dir, sizeof(lwin.curr_dir), "fake/path");
	assert_int_equal(SK_BY_NAME, lwin.sort[0]);
	assert_int_equal(-SK_BY_DIR, lwin.sort[1]);
	assert_int_equal(SK_NONE, lwin.sort[2]);

	opt_handlers_teardown();

	assert_success(remove("vifminfo"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
