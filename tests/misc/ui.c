#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h> /* strcpy() strdup() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/ui/colored_line.h"
#include "../../src/ui/tabs.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"

static void check_tab_title(const tab_info_t *tab_info, const char text[]);
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
	check_tab_title(&tab_info, "name");
}

TEST(make_tab_title_uses_path_if_name_is_missing_and_no_format)
{
	update_string(&cfg.tab_label, "");
	strcpy(lwin.curr_dir, "/lpath");
	tab_info_t tab_info = { .view = &lwin, .name = NULL, };
	check_tab_title(&tab_info, "/lpath");
}

TEST(make_tab_title_uses_format_in_regular_view)
{
	update_string(&cfg.tab_label, "tail:%p:t");
	strcpy(lwin.curr_dir, "/lpath/ltail");
	tab_info_t tab_info = { .view = &lwin, .name = NULL, };
	check_tab_title(&tab_info, "tail:ltail");
}

TEST(make_tab_title_uses_format_in_custom_view)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", NULL);
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	update_string(&cfg.tab_label, "!%c!%p:t");
	tab_info_t tab_info = { .view = &lwin, .name = NULL, };
	check_tab_title(&tab_info, "!test!test-data");
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
	check_tab_title(&tab_info, "!!");
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
	check_tab_title(&tab_info, "!a!");

	lwin.explore_mode = 0;
}

TEST(make_tab_expands_tab_number)
{
	update_string(&cfg.tab_label, "%N");
	tab_info_t tab_info = { .view = &lwin, .name = "name", };
	check_tab_title(&tab_info, "1");
}

TEST(make_tab_expands_current_flag)
{
	update_string(&cfg.tab_label, "%1*%[%2*%C%]%N");
	tab_info_t tab_info = { .view = &lwin, .name = "name", };
	tab_title_info_t title_info;
	cline_t title;

	title_info = make_tab_title_info(&tab_info, &identity, 2, 1);
	title = make_tab_title(&title_info);
	dispose_tab_title_info(&title_info);

	assert_string_equal("3", title.line);
	assert_string_equal("2", title.attrs);
	cline_dispose(&title);

	title_info = make_tab_title_info(&tab_info, &identity, 1, 0);
	title = make_tab_title(&title_info);
	dispose_tab_title_info(&title_info);

	assert_string_equal("2", title.line);
	assert_string_equal("1", title.attrs);
	cline_dispose(&title);
}

TEST(tabline_formatting_smoke)
{
	curr_view = &lwin;
	other_view = &rwin;
	setup_grid(&lwin, 1, 1, 1);
	view_setup(&rwin);
	setup_grid(&rwin, 1, 1, 1);
	curr_stats.load_stage = 2;

	cfg.columns = 10;
	opt_handlers_setup();
	columns_setup_column(SK_BY_NAME);
	columns_setup_column(SK_BY_SIZE);

	tabs_new("long tab title", NULL);
	ui_view_title_update(&lwin);
	tabs_only(&lwin);

	opt_handlers_teardown();
	columns_teardown();

	curr_stats.load_stage = 0;
	view_teardown(&rwin);
	curr_view = NULL;
	other_view = NULL;
}

static void
check_tab_title(const tab_info_t *tab_info, const char text[])
{
	tab_title_info_t title_info = make_tab_title_info(tab_info, &identity, 0, 0);
	cline_t title = make_tab_title(&title_info);
	dispose_tab_title_info(&title_info);

	assert_string_equal(text, title.line);
	cline_dispose(&title);
}

static char *
identity(const char path[])
{
	return (char *)path;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
