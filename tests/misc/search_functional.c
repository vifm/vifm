#include <stic.h>

#include <unistd.h> /* chdir() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/statusbar.h"
#include "../../src/utils/fs.h"
#include "../../src/filelist.h"
#include "../../src/status.h"

static char *saved_cwd;

SETUP_ONCE()
{
	curr_view = &lwin;
	other_view = &rwin;
}

TEARDOWN_ONCE()
{
	curr_view = NULL;
	other_view = NULL;
}

SETUP()
{
	saved_cwd = save_cwd();

	view_setup(&lwin);
	modes_init();
	opt_handlers_setup();

	assert_success(chdir(TEST_DATA_PATH "/read"));
	assert_non_null(get_cwd(lwin.curr_dir, sizeof(lwin.curr_dir)));

	cfg_resize_histories(10);

	populate_dir_list(&lwin, /*reload=*/0);

	assert_int_equal(6, lwin.list_rows);
	assert_int_equal(0, lwin.list_pos);

	cfg.hl_search = 0;
	cfg.inc_search = 0;
}

TEARDOWN()
{
	cfg_resize_histories(0);

	opt_handlers_teardown();
	(void)vle_keys_exec_timed_out(WK_C_c);
	vle_keys_reset();
	view_teardown(&lwin);

	restore_cwd(saved_cwd);
}

TEST(n_works_without_prior_search_in_visual_mode)
{
	hists_search_save("asdfasdfasdf");

	ui_sb_msg("");

	(void)vle_keys_exec_timed_out(WK_v WK_n);

	assert_string_equal("Search hit BOTTOM without match for: asdfasdfasdf",
			ui_sb_last());
	assert_int_equal(0, lwin.list_pos);

	hists_search_save(".");

	ui_sb_msg("");

	(void)vle_keys_exec_timed_out(WK_n);

	assert_string_equal("(2 of 6) /.", ui_sb_last());
	assert_int_equal(1, lwin.list_pos);
	assert_true(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);
	assert_false(lwin.dir_entry[2].selected);
}

TEST(hlsearch_is_not_reset_for_invalid_pattern_during_incsearch_in_visual_mode)
{
	cfg.hl_search = 1;
	cfg.inc_search = 1;

	(void)vle_keys_exec_timed_out(WK_v WK_SLASH L"*");
	assert_true(cfg.hl_search);

	(void)vle_keys_exec_timed_out(WK_C_c);
}

TEST(message_for_invalid_pattern)
{
	ui_sb_msg("");

	(void)vle_keys_exec_timed_out(WK_SLASH L"*" WK_CR);

	assert_string_starts_with("Regexp (*) error: ", ui_sb_last());
}

TEST(selection_is_dropped_for_hlsearch)
{
	cfg.hl_search = 1;

	lwin.dir_entry[0].selected = 1;

	(void)vle_keys_exec_timed_out(WK_SLASH L"dos" WK_CR);
	assert_false(lwin.dir_entry[0].selected);
}

TEST(selection_is_not_dropped_for_nohlsearch)
{
	cfg.hl_search = 0;

	lwin.dir_entry[0].selected = 1;

	(void)vle_keys_exec_timed_out(WK_SLASH L"dos" WK_CR);
	assert_true(lwin.dir_entry[0].selected);
}

TEST(selection_is_not_dropped_in_visual_mode_regardless_of_hlsearch)
{
	cfg.hl_search = 1;

	lwin.dir_entry[0].selected = 1;

	(void)vle_keys_exec_timed_out(WK_a WK_v WK_SLASH L"dos" WK_CR);
	assert_true(lwin.dir_entry[0].selected);

	cfg.hl_search = 0;

	lwin.dir_entry[0].selected = 1;

	(void)vle_keys_exec_timed_out(WK_a WK_v WK_SLASH L"dos" WK_CR);
	assert_true(lwin.dir_entry[0].selected);
}

TEST(selection_on_n_with_hlsearch)
{
	cfg.hl_search = 1;

	hists_search_save("dos");

	/* Selection IS dropped on the first "n". */

	lwin.dir_entry[0].selected = 1;

	(void)vle_keys_exec_timed_out(WK_n);

	assert_int_equal(1, lwin.list_pos);
	assert_false(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);
	assert_true(lwin.dir_entry[2].selected);

	/* Selection IS NOT dropped on the second "n". */

	lwin.dir_entry[0].selected = 1;

	(void)vle_keys_exec_timed_out(WK_n);

	assert_int_equal(2, lwin.list_pos);
	assert_true(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);
	assert_true(lwin.dir_entry[2].selected);
}

TEST(correct_match_number_is_shown_for_search_in_visual_mode)
{
	assert_false(lwin.dir_entry[0].selected);

	ui_sb_msg("");

	(void)vle_keys_exec_timed_out(WK_v WK_SLASH L"dos" WK_CR);

	assert_int_equal(2, lwin.matches);
	assert_int_equal(1, lwin.list_pos);
	assert_int_equal(0, lwin.dir_entry[0].search_match);
	assert_int_equal(1, lwin.dir_entry[1].search_match);
	assert_int_equal(2, lwin.dir_entry[2].search_match);
	assert_string_starts_with("1 of 2 matching files", ui_sb_last());
}

TEST(correct_cursor_position_for_incsearch_with_a_count)
{
	cfg.inc_search = 1;

	(void)vle_keys_exec_timed_out(L"3" WK_SLASH L".." WK_CR);
	assert_int_equal(3, lwin.list_pos);
}

TEST(failed_search_with_a_count_does_not_move_cursor)
{
	cfg.wrap_scan = 0;

	(void)vle_keys_exec_timed_out(L"333" WK_SLASH L"." WK_CR);
	assert_int_equal(0, lwin.list_pos);
}

TEST(correct_message_and_cursor_position_after_failed_incsearch_with_a_count)
{
	cfg.wrap_scan = 0;
	cfg.inc_search = 1;

	ui_sb_msg("");

	(void)vle_keys_exec_timed_out(L"333" WK_SLASH L"." WK_CR);

	assert_string_equal("Search hit BOTTOM without match for: .", ui_sb_last());
	assert_int_equal(0, lwin.list_pos);
}

TEST(message_is_shown_after_incsearch_with_hlsearch_in_visual_mode)
{
	cfg.hl_search = 1;
	cfg.inc_search = 1;

	ui_sb_msg("");

	(void)vle_keys_exec_timed_out(WK_v WK_SLASH L"." WK_CR);

	assert_string_starts_with("2 of 6 matching files", ui_sb_last());
}

TEST(selection_isnt_dropped_on_empty_input_during_incsearch_with_hls_in_vismode)
{
	cfg.hl_search = 1;
	cfg.inc_search = 1;

	(void)vle_keys_exec_timed_out(WK_v WK_SLASH);

	assert_int_equal(0, lwin.list_pos);
	assert_true(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);

	(void)vle_keys_exec_timed_out(L"." WK_C_u);

	assert_int_equal(0, lwin.list_pos);
	assert_true(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);

	(void)vle_keys_exec_timed_out(WK_C_c);
}

TEST(selection_is_not_leaved_on_cursor_backtracking_during_incsearch_in_vismode)
{
	cfg.inc_search = 1;

	(void)vle_keys_exec_timed_out(WK_v WK_SLASH L"dos");

	assert_int_equal(1, lwin.list_pos);
	assert_true(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);

	(void)vle_keys_exec_timed_out(L"asdfasdfasdf");

	assert_int_equal(0, lwin.list_pos);
	assert_true(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);

	(void)vle_keys_exec_timed_out(WK_C_c);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
