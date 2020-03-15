#include <stic.h>

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() */
#include <string.h> /* strcpy() strdup() */

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/engine/keys.h"
#include "../../src/modes/cmdline.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
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
	modcline_enter(CLS_COMMAND, "", NULL);

	curr_view = &lwin;
	view_setup(&lwin);

	char *error;
	matcher_free(curr_view->manual_filter);
	assert_non_null(curr_view->manual_filter =
			matcher_alloc("{filt}", 0, 0, "", &error));
	assert_success(filter_set(&curr_view->auto_filter, "auto-filter"));
	assert_success(filter_set(&curr_view->local_filter.filter, "local-filter"));

	lwin.list_pos = 0;
	lwin.list_rows = 2;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("\265");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].name = strdup("root.ext");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];
	strcpy(lwin.curr_dir, "tests/fake/tail");

	other_view = &rwin;
	view_setup(&rwin);

	rwin.list_pos = 0;
	rwin.list_rows = 1;
	rwin.dir_entry = dynarray_cextend(NULL,
			rwin.list_rows*sizeof(*rwin.dir_entry));
	rwin.dir_entry[0].name = strdup("otherroot.otherext");
	rwin.dir_entry[0].origin = &rwin.curr_dir[0];
	strcpy(rwin.curr_dir, "other/dir/othertail");
}

TEARDOWN()
{
	view_teardown(&lwin);
	view_teardown(&rwin);

	(void)vle_keys_exec_timed_out(WK_C_c);

	vle_keys_reset();
}

TEST(value_of_manual_filter_is_pasted)
{
	(void)vle_keys_exec_timed_out(WK_C_x WK_m);
	assert_wstring_equal(L"filt", stats->line);
}

TEST(value_of_automatic_filter_is_pasted)
{
	(void)vle_keys_exec_timed_out(WK_C_x WK_a);
	assert_wstring_equal(L"auto-filter", stats->line);
}

TEST(value_of_local_filter_is_pasted)
{
	(void)vle_keys_exec_timed_out(WK_C_x WK_EQUALS);
	assert_wstring_equal(L"local-filter", stats->line);
}

TEST(last_search_pattern_is_pasted)
{
	/* Emulate proper history initialization (must happen after view
	 * initialization). */
	cfg_resize_histories(5);
	cfg_resize_histories(0);

	cfg_resize_histories(5);
	hists_search_save("search-pattern");

	(void)vle_keys_exec_timed_out(WK_C_x WK_SLASH);
	assert_wstring_equal(L"search-pattern", stats->line);

	cfg_resize_histories(0);
}

TEST(name_of_current_file_is_pasted)
{
	lwin.list_pos = 1;

	(void)vle_keys_exec_timed_out(WK_C_x WK_c);
	assert_wstring_equal(L"root.ext", stats->line);

	(void)vle_keys_exec_timed_out(WK_SPACE);

	(void)vle_keys_exec_timed_out(WK_C_x WK_C_x WK_c);
	assert_wstring_equal(L"root.ext otherroot.otherext", stats->line);
}

TEST(short_path_of_current_file_is_pasted_in_cv)
{
	char cwd[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", cwd);

	char path[PATH_MAX + 1];
	flist_custom_start(&lwin, "test");
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "existing-files/a", cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "existing-files/b", cwd);
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	(void)vle_keys_exec_timed_out(WK_C_x WK_c);

	assert_wstring_equal(L"existing-files/a", stats->line);
}

TEST(root_is_pasted)
{
	lwin.list_pos = 1;

	(void)vle_keys_exec_timed_out(WK_C_x WK_r);
	assert_wstring_equal(L"root", stats->line);

	(void)vle_keys_exec_timed_out(WK_SPACE);

	(void)vle_keys_exec_timed_out(WK_C_x WK_C_x WK_r);
	assert_wstring_equal(L"root otherroot", stats->line);
}

TEST(extension_is_pasted)
{
	lwin.list_pos = 1;

	(void)vle_keys_exec_timed_out(WK_C_x WK_e);
	assert_wstring_equal(L"ext", stats->line);

	(void)vle_keys_exec_timed_out(WK_SPACE);

	(void)vle_keys_exec_timed_out(WK_C_x WK_C_x WK_e);
	assert_wstring_equal(L"ext otherext", stats->line);
}

TEST(directory_is_pasted)
{
	(void)vle_keys_exec_timed_out(WK_C_x WK_d);
	assert_wstring_equal(L"tests/fake/tail", stats->line);

	(void)vle_keys_exec_timed_out(WK_SPACE);

	(void)vle_keys_exec_timed_out(WK_C_x WK_C_x WK_d);
	assert_wstring_equal(L"tests/fake/tail other/dir/othertail", stats->line);
}

TEST(tail_of_current_directory_is_pasted)
{
	(void)vle_keys_exec_timed_out(WK_C_x WK_t);
	assert_wstring_equal(L"tail", stats->line);

	(void)vle_keys_exec_timed_out(WK_SPACE);

	(void)vle_keys_exec_timed_out(WK_C_x WK_C_x WK_t);
	assert_wstring_equal(L"tail othertail", stats->line);
}

TEST(broken_utf8_name, IF(utf8_locale))
{
	(void)vle_keys_exec_timed_out(WK_C_x WK_c);

	assert_true(stats->line != NULL && stats->line[0] != L'\0');
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
