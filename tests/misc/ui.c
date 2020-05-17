#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h> /* strcpy() strdup() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/ui/tabs.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"

static char * identity(const char path[]);

SETUP_ONCE()
{
	cfg.shorten_title_paths = 0;
	cfg.tail_tab_line_paths = 0;
}

SETUP()
{
	conf_setup();
	view_setup(&lwin);
}

TEARDOWN()
{
	conf_teardown();
	view_teardown(&lwin);
}

TEST(make_tab_title_uses_name_if_present_and_no_format)
{
	update_string(&cfg.tab_label, "");
	tab_info_t tab_info = { .view = &lwin, .name = "name", };
	char *title = make_tab_title(&tab_info, &identity);
	assert_string_equal("name", title);
	free(title);
}

TEST(make_tab_title_uses_path_if_name_is_missing_and_no_format)
{
	update_string(&cfg.tab_label, "");
	strcpy(lwin.curr_dir, "/lpath");
	tab_info_t tab_info = { .view = &lwin, .name = NULL, };
	char *title = make_tab_title(&tab_info, &identity);
	assert_string_equal("/lpath", title);
	free(title);
}

TEST(make_tab_title_uses_format_in_regular_view)
{
	update_string(&cfg.tab_label, "tail:%p:t");
	strcpy(lwin.curr_dir, "/lpath/ltail");
	tab_info_t tab_info = { .view = &lwin, .name = NULL, };
	char *title = make_tab_title(&tab_info, &identity);
	assert_string_equal("tail:ltail", title);
	free(title);
}

TEST(make_tab_title_uses_format_in_custom_view)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", NULL);
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	update_string(&cfg.tab_label, "!%c!%p:t");
	tab_info_t tab_info = { .view = &lwin, .name = NULL, };
	char *title = make_tab_title(&tab_info, &identity);
	assert_string_equal("!test!test-data", title);
	free(title);
}

TEST(make_tab_title_uses_format_after_custom_view)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", NULL);
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);
	navigate_to(&lwin, TEST_DATA_PATH);

	update_string(&cfg.tab_label, "!%c!");
	tab_info_t tab_info = { .view = &lwin, .name = NULL, };
	char *title = make_tab_title(&tab_info, &identity);
	assert_string_equal("!!", title);
	free(title);
}

TEST(make_tab_title_handles_explore_mode_for_format)
{
	lwin.explore_mode = 1;

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", NULL);
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	update_string(&cfg.tab_label, "!%p:t!");
	tab_info_t tab_info = { .view = &lwin, .name = NULL, };
	char *title = make_tab_title(&tab_info, &identity);
	assert_string_equal("!a!", title);
	free(title);

	lwin.explore_mode = 0;
}

static char *
identity(const char path[])
{
	return (char *)path;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
