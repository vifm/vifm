#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../../src/engine/keys.h"
#include "../../src/modes/cmdline.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/matcher.h"
#include "../../src/filelist.h"

#include "utils.h"

static line_stats_t *stats;

SETUP_ONCE()
{
	stats = get_line_stats();
	try_enable_utf8_locale();
}

SETUP()
{
	init_modes();
	enter_cmdline_mode(CLS_COMMAND, "", NULL);

	curr_view = &lwin;
	view_setup(&lwin);

	char *error;
	matcher_free(curr_view->manual_filter);
	assert_non_null(curr_view->manual_filter =
			matcher_alloc("{filt}", 0, 0, "", &error));

	lwin.list_pos = 0;
	lwin.list_rows = 1;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("\265");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
}

TEARDOWN()
{
	view_teardown(&lwin);

	(void)vle_keys_exec_timed_out(WK_C_c);

	vle_keys_reset();
}

TEST(value_of_manual_filter_is_pasted)
{
	(void)vle_keys_exec_timed_out(WK_C_x WK_m);

	assert_wstring_equal(L"filt", stats->line);
}

TEST(broken_utf8_name, IF(utf8_locale))
{
	(void)vle_keys_exec_timed_out(WK_C_x WK_c);

	assert_wstring_equal(L"?", stats->line);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
