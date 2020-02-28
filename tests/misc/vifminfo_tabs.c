#include <stic.h>

#include <stdio.h> /* remove() */

#include "../../src/compat/fs_limits.h"
#include "../../src/cfg/config.h"
#include "../../src/cfg/info.h"
#include "../../src/ui/tabs.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"

#include "utils.h"

SETUP_ONCE()
{
	char cwd[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", cwd);

	columns_setup_column(SK_BY_NAME);
	columns_setup_column(SK_BY_SIZE);

	curr_view = &lwin;
	other_view = &rwin;
}

TEARDOWN_ONCE()
{
	columns_teardown();

	curr_view = NULL;
	other_view = NULL;
}

SETUP()
{
	view_setup(&lwin);
	view_setup(&rwin);

	opt_handlers_setup();

	init_view_list(&lwin);
	init_view_list(&rwin);
}

TEARDOWN()
{
	opt_handlers_teardown();

	view_teardown(&lwin);
	view_teardown(&rwin);

	cfg.pane_tabs = 0;
	tabs_only(&lwin);

	assert_success(remove(SANDBOX_PATH "/vifminfo.json"));
}

TEST(names_of_global_tabs_are_restored)
{
	tabs_rename(&lwin, "gtab0");
	assert_success(tabs_new("gtab1", NULL));
	assert_success(tabs_new("gtab2", NULL));

	write_info_file();
	tabs_only(&lwin);
	tabs_rename(&lwin, NULL);
	read_info_file(0);

	assert_int_equal(3, tabs_count(&lwin));
	tab_info_t tab_info;
	assert_true(tabs_enum(&lwin, 0, &tab_info));
	assert_string_equal("gtab0", tab_info.name);
	assert_true(tabs_enum(&lwin, 1, &tab_info));
	assert_string_equal("gtab1", tab_info.name);
	assert_true(tabs_enum(&lwin, 2, &tab_info));
	assert_string_equal("gtab2", tab_info.name);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
