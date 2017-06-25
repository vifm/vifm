#include <stic.h>

#include <sys/stat.h> /* stat */
#include <unistd.h> /* stat() */

#include <stdio.h> /* fclose() fopen() fprintf() remove() */

#include "../../src/cfg/config.h"
#include "../../src/cfg/info.h"
#include "../../src/cfg/info_chars.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/matcher.h"
#include "../../src/utils/matchers.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/filetype.h"
#include "../../src/opt_handlers.h"

#include "utils.h"

SETUP()
{
	view_setup(&lwin);
	view_setup(&rwin);
}

TEARDOWN()
{
	view_teardown(&lwin);
	view_teardown(&rwin);
}

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

	assert_success(remove(SANDBOX_PATH "/vifminfo"));
}

TEST(filetypes_are_deduplicated)
{
	struct stat first, second;
	char *error;
	matchers_t *ms;

	copy_str(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH);
	cfg.vifm_info = VIFMINFO_FILETYPES;
	init_commands();

	/* Add a filetype. */
	ms = matchers_alloc("*.c", 0, 1, "", &error);
	assert_non_null(ms);
	ft_set_programs(ms, "{Description}com,,mand,{descr2}cmd", 0, 1);

	/* Write it first time. */
	write_info_file();
	/* And remember size of the file. */
	assert_success(stat(SANDBOX_PATH "/vifminfo", &first));

	/* Add filetype again (as if it was read from vifmrc). */
	ms = matchers_alloc("*.c", 0, 1, "", &error);
	assert_non_null(ms);
	ft_set_programs(ms, "{Description}com,,mand,{descr2}cmd", 0, 1);

	/* Update vifminfo second time. */
	write_info_file();
	/* Check that size hasn't changed. */
	assert_success(stat(SANDBOX_PATH "/vifminfo", &second));
	assert_true(first.st_size == second.st_size);

	assert_success(remove(SANDBOX_PATH "/vifminfo"));
	reset_cmds();
}

TEST(correct_manual_filters_are_read_from_vifminfo)
{
	FILE *const f = fopen(SANDBOX_PATH "/vifminfo", "w");
	fprintf(f, "%c%s\n", LINE_TYPE_LWIN_FILT, "abc");
	fprintf(f, "%c%s\n", LINE_TYPE_RWIN_FILT, "cba");
	fclose(f);

	copy_str(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH);
	read_info_file(1);

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

	copy_str(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH);
	read_info_file(1);

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

	copy_str(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH);
	read_info_file(1);

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

	copy_str(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH);
	read_info_file(1);

	assert_int_equal(2, lwin.history_pos);
	/* First entry is correct. */
	assert_int_equal(11, lwin.history[1].rel_pos);
	/* Second entry is not read in full. */
	assert_int_equal(0, lwin.history[2].rel_pos);

	assert_success(remove(SANDBOX_PATH "/vifminfo"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
