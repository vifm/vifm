#include <stic.h>

#include <sys/stat.h> /* stat */
#include <unistd.h> /* stat() */

#include <stdio.h> /* fclose() fopen() fprintf() remove() */
#include <stdlib.h> /* free() */
#include <string.h> /* memset() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/cfg/info.h"
#include "../../src/cfg/info_chars.h"
#include "../../src/ui/column_view.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/matcher.h"
#include "../../src/utils/matchers.h"
#include "../../src/utils/parson.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/filetype.h"
#include "../../src/flist_hist.h"
#include "../../src/opt_handlers.h"
#include "../../src/status.h"

SETUP_ONCE()
{
	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", NULL);
}

TEARDOWN_ONCE()
{
	cfg.config_dir[0] = '\0';
}

SETUP()
{
	view_setup(&lwin);
	view_setup(&rwin);
	curr_view = &lwin;

	cfg_resize_histories(10);

	cfg.vifm_info = 0;
}

TEARDOWN()
{
	cfg_resize_histories(0);

	view_teardown(&lwin);
	view_teardown(&rwin);

	cfg.vifm_info = 0;
}

TEST(view_sorting_is_read_from_vifminfo)
{
	FILE *const f = fopen(SANDBOX_PATH "/vifminfo", "w");
	fprintf(f, "%c%d,%d", LINE_TYPE_LWIN_SORT, SK_BY_NAME, -SK_BY_DIR);
	fclose(f);

	/* ls-like view blocks view column updates. */
	lwin.ls_view = 1;
	state_load(1);
	lwin.ls_view = 0;

	opt_handlers_setup();

	copy_str(lwin.curr_dir, sizeof(lwin.curr_dir), "fake/path");
	assert_int_equal(SK_BY_NAME, lwin.sort[0]);
	assert_int_equal(-SK_BY_DIR, lwin.sort[1]);
	assert_int_equal(SK_NONE, lwin.sort[2]);

	opt_handlers_teardown();

	assert_success(remove(SANDBOX_PATH "/vifminfo"));
}

TEST(filetypes_are_deduplicated)
{
	struct stat first, second;
	char *error;
	matchers_t *ms;

	cfg.vifm_info = VINFO_FILETYPES;
	init_commands();

	/* Add a filetype. */
	ms = matchers_alloc("*.c", 0, 1, "", &error);
	assert_non_null(ms);
	ft_set_programs(ms, "{Description}com,,mand,{descr2}cmd", 0, 1);

	/* Write it first time. */
	write_info_file();
	/* And remember size of the file. */
	assert_success(stat(SANDBOX_PATH "/vifminfo.json", &first));

	/* Add filetype again (as if it was read from vifmrc). */
	ms = matchers_alloc("*.c", 0, 1, "", &error);
	assert_non_null(ms);
	ft_set_programs(ms, "{Description}com,,mand,{descr2}cmd", 0, 1);

	/* Update vifminfo second time. */
	write_info_file();
	/* Check that size hasn't changed. */
	assert_success(stat(SANDBOX_PATH "/vifminfo.json", &second));
	assert_true(first.st_size == second.st_size);

	assert_success(remove(SANDBOX_PATH "/vifminfo.json"));
	vle_cmds_reset();
}

TEST(correct_manual_filters_are_read_from_vifminfo)
{
	FILE *const f = fopen(SANDBOX_PATH "/vifminfo", "w");
	fprintf(f, "%c%s\n", LINE_TYPE_LWIN_FILT, "abc");
	fprintf(f, "%c%s\n", LINE_TYPE_RWIN_FILT, "cba");
	fclose(f);

	state_load(1);

	assert_string_equal("abc", lwin.prev_manual_filter);
	assert_string_equal("abc", matcher_get_expr(lwin.manual_filter));
	assert_string_equal("cba", rwin.prev_manual_filter);
	assert_string_equal("cba", matcher_get_expr(rwin.manual_filter));

	assert_success(remove(SANDBOX_PATH "/vifminfo"));
}

TEST(incorrect_manual_filters_in_vifminfo_are_cleared)
{
	FILE *const f = fopen(SANDBOX_PATH "/vifminfo", "w");
	fprintf(f, "%c%s\n", LINE_TYPE_LWIN_FILT, "*");
	fprintf(f, "%c%s\n", LINE_TYPE_RWIN_FILT, "?");
	fclose(f);

	state_load(1);

	assert_string_equal("", lwin.prev_manual_filter);
	assert_string_equal("", matcher_get_expr(lwin.manual_filter));
	assert_string_equal("", rwin.prev_manual_filter);
	assert_string_equal("", matcher_get_expr(rwin.manual_filter));

	assert_success(remove(SANDBOX_PATH "/vifminfo"));
}

TEST(file_with_newline_and_dash_in_history_does_not_cause_abort)
{
	FILE *const f = fopen(SANDBOX_PATH "/vifminfo", "w");
	fputs("d/dev/shm/TEES/out\n", f);
	fputs("\tAVxXQ1xDFJDzUCRLgIob\n-log.txt\n", f);
	fputs("10\n", f);
	fclose(f);

	cfg_resize_histories(1);

	state_load(1);

	assert_success(remove(SANDBOX_PATH "/vifminfo"));
}

TEST(optional_number_should_not_be_preceded_by_a_whitespace)
{
	FILE *const f = fopen(SANDBOX_PATH "/vifminfo", "w");
	fputs("d/dev/shm/TEES/out1\n", f);
	fputs("\tAVxXQ1xDFJDzUCRLgIob.log\n", f);
	fputs("11\n", f);
	fputs("d/dev/shm/TEES/out\n", f);
	fputs("\tAVxXQ1xDFJDzUCRLgIob-log.txt\n", f);
	fputs(" 10\n", f);
	fclose(f);

	state_load(1);

	assert_int_equal(1, lwin.history_pos);
	/* First entry is correct. */
	assert_int_equal(11, lwin.history[0].rel_pos);
	/* Second entry is not read in full. */
	assert_int_equal(0, lwin.history[1].rel_pos);

	assert_success(remove(SANDBOX_PATH "/vifminfo"));
}

TEST(history_is_automatically_extended)
{
	FILE *const f = fopen(SANDBOX_PATH "/vifminfo", "w");
	fputs("d/path1\n\tfile1\n", f);
	fputs("d/path2\n\tfile2\n", f);
	fputs("d/path1\n\tfile1\n", f);
	fputs("d/path2\n\tfile2\n", f);
	fputs("d/path1\n\tfile1\n", f);
	fputs("d/path2\n\tfile2\n", f);
	fputs("d/path1\n\tfile1\n", f);
	fputs("d/path2\n\tfile2\n", f);
	fputs("d/path1\n\tfile1\n", f);
	fputs("d/path2\n\tfile2\n", f);
	fputs("d/path1\n\tfile1\n", f);
	fputs("d/path2\n\tfile2\n", f);
	fclose(f);

	state_load(1);

	assert_int_equal(12, lwin.history_num);

	assert_success(remove(SANDBOX_PATH "/vifminfo"));
}

TEST(empty_vifminfo_option_produces_empty_state)
{
	JSON_Value *value = serialize_state(0);
	char *as_string = json_serialize_to_string(value);

	assert_string_equal("{\"gtabs\":["
	                         "{\"panes\":[{\"ptabs\":[{}]},{\"ptabs\":[{}]}]}"
	                       "]"
	                    "}", as_string);

	free(as_string);
	json_value_free(value);
}

TEST(histories_are_merged_correctly)
{
	cfg.vifm_info = VINFO_CHISTORY | VINFO_SHISTORY | VINFO_PHISTORY
	              | VINFO_FHISTORY | VINFO_EHISTORY;

	hist_add(&curr_stats.cmd_hist, "command0", 0);
	hist_add(&curr_stats.cmd_hist, "command2", 2);
	hist_add(&curr_stats.search_hist, "search0", 0);
	hist_add(&curr_stats.search_hist, "search2", 2);
	hist_add(&curr_stats.prompt_hist, "prompt0", 0);
	hist_add(&curr_stats.prompt_hist, "prompt2", 2);
	hist_add(&curr_stats.filter_hist, "lfilter0", 0);
	hist_add(&curr_stats.filter_hist, "lfilter2", 2);
	hist_add(&curr_stats.exprreg_hist, "exprreg0", 0);
	hist_add(&curr_stats.exprreg_hist, "exprreg2", 2);

	/* First time, no merging is necessary. */
	write_info_file();

	hist_add(&curr_stats.cmd_hist, "command1", 1);
	hist_add(&curr_stats.search_hist, "search1", 1);
	hist_add(&curr_stats.prompt_hist, "prompt1", 1);
	hist_add(&curr_stats.filter_hist, "lfilter1", 1);
	hist_add(&curr_stats.exprreg_hist, "exprreg1", 1);

	/* Second time, touched vifminfo.json file, merging is necessary. */
	reset_timestamp(SANDBOX_PATH "/vifminfo.json");
	write_info_file();

	/* Clear histories. */
	cfg_resize_histories(0);
	cfg_resize_histories(10);

	state_load(0);

	assert_int_equal(3, curr_stats.cmd_hist.size);
	assert_int_equal(3, curr_stats.search_hist.size);
	assert_int_equal(3, curr_stats.prompt_hist.size);
	assert_int_equal(3, curr_stats.filter_hist.size);
	assert_int_equal(3, curr_stats.exprreg_hist.size);
	assert_string_equal("command2", curr_stats.cmd_hist.items[0].text);
	assert_int_equal(2, curr_stats.cmd_hist.items[0].timestamp);
	assert_string_equal("command1", curr_stats.cmd_hist.items[1].text);
	assert_int_equal(1, curr_stats.cmd_hist.items[1].timestamp);
	assert_string_equal("command0", curr_stats.cmd_hist.items[2].text);
	assert_int_equal(0, curr_stats.cmd_hist.items[2].timestamp);
	assert_string_equal("search2", curr_stats.search_hist.items[0].text);
	assert_int_equal(2, curr_stats.search_hist.items[0].timestamp);
	assert_string_equal("search1", curr_stats.search_hist.items[1].text);
	assert_int_equal(1, curr_stats.search_hist.items[1].timestamp);
	assert_string_equal("search0", curr_stats.search_hist.items[2].text);
	assert_int_equal(0, curr_stats.search_hist.items[2].timestamp);
	assert_string_equal("prompt2", curr_stats.prompt_hist.items[0].text);
	assert_int_equal(2, curr_stats.prompt_hist.items[0].timestamp);
	assert_string_equal("prompt1", curr_stats.prompt_hist.items[1].text);
	assert_int_equal(1, curr_stats.prompt_hist.items[1].timestamp);
	assert_string_equal("prompt0", curr_stats.prompt_hist.items[2].text);
	assert_int_equal(0, curr_stats.prompt_hist.items[2].timestamp);
	assert_string_equal("lfilter2", curr_stats.filter_hist.items[0].text);
	assert_int_equal(2, curr_stats.filter_hist.items[0].timestamp);
	assert_string_equal("lfilter1", curr_stats.filter_hist.items[1].text);
	assert_int_equal(1, curr_stats.filter_hist.items[1].timestamp);
	assert_string_equal("lfilter0", curr_stats.filter_hist.items[2].text);
	assert_int_equal(0, curr_stats.filter_hist.items[2].timestamp);
	assert_string_equal("exprreg2", curr_stats.exprreg_hist.items[0].text);
	assert_int_equal(2, curr_stats.exprreg_hist.items[0].timestamp);
	assert_string_equal("exprreg1", curr_stats.exprreg_hist.items[1].text);
	assert_int_equal(1, curr_stats.exprreg_hist.items[1].timestamp);
	assert_string_equal("exprreg0", curr_stats.exprreg_hist.items[2].text);
	assert_int_equal(0, curr_stats.exprreg_hist.items[2].timestamp);

	assert_success(remove(SANDBOX_PATH "/vifminfo.json"));
}

TEST(view_sorting_round_trip)
{
	cfg.vifm_info = VINFO_TUI;

	opt_handlers_setup();
	lwin.columns = columns_create();
	rwin.columns = columns_create();
	columns_setup_column(SK_BY_NAME);
	columns_setup_column(SK_BY_SIZE);
	columns_setup_column(SK_BY_NITEMS);
	columns_setup_column(SK_BY_EXTENSION);
	columns_setup_column(SK_BY_DIR);
	columns_setup_column(SK_BY_FILEEXT);
	columns_setup_column(SK_BY_TARGET);
	columns_setup_column(SK_BY_TYPE);
	columns_setup_column(SK_BY_INAME);
	columns_setup_column(SK_BY_TIME_CHANGED);

	write_info_file();
	memset(lwin.sort_g, SK_NONE, sizeof(lwin.sort_g));
	memset(rwin.sort_g, SK_NONE, sizeof(rwin.sort_g));
	state_load(0);

	assert_int_equal(SK_BY_NAME, lwin.sort_g[0]);
	assert_int_equal(SK_BY_NAME, rwin.sort_g[0]);

	lwin.sort_g[0] = SK_BY_NITEMS;
	lwin.sort_g[1] = -SK_BY_EXTENSION;
	lwin.sort_g[2] = SK_BY_SIZE;
	lwin.sort_g[3] = -SK_BY_NAME;
	lwin.sort_g[4] = SK_BY_DIR;

	rwin.sort_g[0] = -SK_BY_TIME_CHANGED;
	rwin.sort_g[1] = SK_BY_TARGET;
	rwin.sort_g[2] = SK_BY_INAME;
	rwin.sort_g[3] = SK_BY_FILEEXT;
	rwin.sort_g[4] = -SK_BY_TYPE;
	rwin.sort_g[5] = -SK_BY_NAME;

	write_info_file();
	memset(lwin.sort_g, SK_NONE, sizeof(lwin.sort_g));
	memset(rwin.sort_g, SK_NONE, sizeof(rwin.sort_g));
	state_load(0);

	assert_int_equal(SK_BY_NITEMS, lwin.sort_g[0]);
	assert_int_equal(-SK_BY_EXTENSION, lwin.sort_g[1]);
	assert_int_equal(SK_BY_SIZE, lwin.sort_g[2]);
	assert_int_equal(-SK_BY_NAME, lwin.sort_g[3]);
	assert_int_equal(SK_BY_DIR, lwin.sort_g[4]);

	assert_int_equal(-SK_BY_TIME_CHANGED, rwin.sort_g[0]);
	assert_int_equal(SK_BY_TARGET, rwin.sort_g[1]);
	assert_int_equal(SK_BY_INAME, rwin.sort_g[2]);
	assert_int_equal(SK_BY_FILEEXT, rwin.sort_g[3]);
	assert_int_equal(-SK_BY_TYPE, rwin.sort_g[4]);
	assert_int_equal(-SK_BY_NAME, rwin.sort_g[5]);

	opt_handlers_teardown();
	columns_free(lwin.columns);
	lwin.columns = NULL;
	columns_free(rwin.columns);
	rwin.columns = NULL;
	columns_teardown();

	assert_success(remove(SANDBOX_PATH "/vifminfo.json"));
}

TEST(view_filters_round_trip)
{
	cfg.vifm_info = VINFO_STATE;
	opt_handlers_setup();

	lwin.invert = 0;
	lwin.hide_dot = 1;
	assert_success(filter_set(&lwin.auto_filter, "lauto-value"));
	assert_success(replace_matcher(&lwin.manual_filter, "^lmanual-value$"));
	rwin.invert = 1;
	rwin.hide_dot = 0;
	assert_success(filter_set(&rwin.auto_filter, "rauto-value"));
	assert_success(replace_matcher(&rwin.manual_filter, "^rmanual-value$"));

	write_info_file();
	lwin.invert = -1;
	lwin.hide_dot = -1;
	assert_success(filter_set(&lwin.auto_filter, "auto-clear"));
	assert_success(replace_matcher(&lwin.manual_filter, "^manual-clear$"));
	rwin.invert = -1;
	rwin.hide_dot = -1;
	assert_success(filter_set(&rwin.auto_filter, "auto-clear"));
	assert_success(replace_matcher(&rwin.manual_filter, "^manual-clear$"));
	state_load(0);

	assert_int_equal(0, lwin.invert);
	assert_int_equal(1, lwin.hide_dot);
	assert_string_equal("lauto-value", lwin.auto_filter.raw);
	assert_string_equal("^lmanual-value$", matcher_get_expr(lwin.manual_filter));
	assert_int_equal(1, rwin.invert);
	assert_int_equal(0, rwin.hide_dot);
	assert_string_equal("rauto-value", rwin.auto_filter.raw);
	assert_string_equal("^rmanual-value$", matcher_get_expr(rwin.manual_filter));

	opt_handlers_teardown();
}

TEST(savedirs_works_on_its_own)
{
	cfg.vifm_info = VINFO_SAVEDIRS;

	copy_str(lwin.curr_dir, sizeof(lwin.curr_dir), "/ldir");
	copy_str(rwin.curr_dir, sizeof(rwin.curr_dir), "/rdir");

	write_info_file();
	lwin.curr_dir[0] = '\0';
	rwin.curr_dir[0] = '\0';
	state_load(0);

	assert_string_equal("/ldir", lwin.curr_dir);
	assert_string_equal("/rdir", rwin.curr_dir);
}

TEST(dhistory_is_merged_correctly)
{
	cfg.vifm_info = VINFO_DHISTORY;

	flist_hist_setup(&lwin, "/dir1", "file5", 5, 5);
	flist_hist_setup(&lwin, "/dir2", "file6", 6, 6);

	/* First time, no merging is necessary. */
	write_info_file();

	/* Clear histories. */
	cfg_resize_histories(0);
	cfg_resize_histories(10);

	flist_hist_setup(&lwin, "/dir3", "file3", 3, 3);
	flist_hist_setup(&lwin, "/dir1", "file4", 4, 4);

	/* Second time, touched vifminfo.json file, merging is necessary. */
	reset_timestamp(SANDBOX_PATH "/vifminfo.json");
	write_info_file();

	/* Clear histories. */
	cfg_resize_histories(0);
	cfg_resize_histories(10);

	state_load(0);

	assert_int_equal(2, lwin.history_pos);
	assert_int_equal(3, lwin.history_num);

	assert_string_equal("/dir3", lwin.history[0].dir);
	assert_string_equal("file3", lwin.history[0].file);
	assert_int_equal(3, lwin.history[0].timestamp);
	assert_int_equal(3, lwin.history[0].rel_pos);
	assert_string_equal("/dir1", lwin.history[1].dir);
	assert_string_equal("file5", lwin.history[1].file);
	assert_int_equal(5, lwin.history[1].timestamp);
	assert_int_equal(5, lwin.history[1].rel_pos);
	assert_string_equal("/dir2", lwin.history[2].dir);
	assert_string_equal("file6", lwin.history[2].file);
	assert_int_equal(6, lwin.history[2].timestamp);
	assert_int_equal(6, lwin.history[2].rel_pos);

	assert_success(remove(SANDBOX_PATH "/vifminfo.json"));
}

TEST(things_missing_from_vifminfo_option_are_dropped)
{
	cfg.vifm_info = VINFO_CS;
	write_info_file();

	reset_timestamp(SANDBOX_PATH "/vifminfo.json");

	cfg.vifm_info = VINFO_TUI | VINFO_STATE;
	write_info_file();

	char *locale = drop_locale();
	JSON_Value *json = json_parse_file(SANDBOX_PATH "/vifminfo.json");
	if(json != NULL)
	{
		assert_false(json_object_has_value(json_object(json), "color-scheme"));
		json_value_free(json);
	}

	restore_locale(locale);

	assert_success(remove(SANDBOX_PATH "/vifminfo.json"));
}

TEST(active_pane_is_respected_both_ways)
{
	curr_view = &lwin;
	other_view = &rwin;

	make_file(SANDBOX_PATH "/vifminfo.json", "{\"gtabs\":[{\"active-pane\":1}]}");
	state_load(0);
	assert_true(curr_view == &rwin);
	assert_true(other_view == &lwin);

	make_file(SANDBOX_PATH "/vifminfo.json", "{\"gtabs\":[{\"active-pane\":0}]}");
	state_load(0);
	assert_true(curr_view == &lwin);
	assert_true(other_view == &rwin);

	remove_file(SANDBOX_PATH "/vifminfo.json");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
