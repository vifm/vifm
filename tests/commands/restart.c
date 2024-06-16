#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/keys.h"
#include "../../src/ui/fileview.h"
#include "../../src/ui/column_view.h"
#include "../../src/ui/tabs.h"
#include "../../src/ui/ui.h"
#include "../../src/cmd_core.h"
#include "../../src/flist_hist.h"
#include "../../src/status.h"

static void setup_tabs(void);
static void check_first_tab(void);
static void check_second_tab(void);

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;

	view_setup(&lwin);
	setup_grid(&lwin, 1, 1, 1);
	view_setup(&rwin);
	setup_grid(&rwin, 1, 1, 1);

	lwin.columns = columns_create();
	rwin.columns = columns_create();
	columns_setup_column(SK_BY_NAME);
	columns_setup_column(SK_BY_SIZE);

	cmds_init();
	cfg.history_len = 10;
	opt_handlers_setup();
}

TEARDOWN()
{
	tabs_only(&lwin);
	tabs_only(&rwin);

	columns_teardown();

	opt_handlers_teardown();
	cfg.history_len = 0;
	vle_keys_reset();

	view_teardown(&lwin);
	view_teardown(&rwin);

	curr_view = NULL;
	other_view = NULL;
}

TEST(history_survives_in_tabs_on_restart_without_persistance)
{
	histories_init(10);

	setup_tabs();

	assert_success(cmds_dispatch("restart", &lwin, CIT_COMMAND));

	check_second_tab();
	assert_success(cmds_dispatch("tabprev", &lwin, CIT_COMMAND));
	check_first_tab();
}

TEST(history_works_after_restart)
{
	histories_init(10);

	hists_commands_save("cmd-1");
	hists_commands_save("cmd-2");

	assert_int_equal(10, curr_stats.cmd_hist.capacity);
	assert_int_equal(2, curr_stats.cmd_hist.size);
	assert_string_equal("cmd-2", curr_stats.cmd_hist.items[0].text);
	assert_string_equal("cmd-1", curr_stats.cmd_hist.items[1].text);

	assert_success(cmds_dispatch("restart", &lwin, CIT_COMMAND));
	assert_int_equal(10, cfg.history_len);

	hists_commands_save("cmd-3");

	assert_int_equal(10, curr_stats.cmd_hist.capacity);
	assert_int_equal(1, curr_stats.cmd_hist.size);
	assert_string_equal("cmd-3", curr_stats.cmd_hist.items[0].text);
}

TEST(restart_checks_its_parameter)
{
	assert_failure(cmds_dispatch("restart wrong", &lwin, CIT_COMMAND));
}

TEST(history_survives_in_tabs_on_restart_with_persistance)
{
	histories_init(10);
	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", NULL);
	cfg.vifm_info = VINFO_DHISTORY | VINFO_TABS;

	setup_tabs();

	assert_success(cmds_dispatch("write | restart", &lwin, CIT_COMMAND));

	check_second_tab();
	assert_success(cmds_dispatch("tabprev", &lwin, CIT_COMMAND));
	check_first_tab();

	remove_file(SANDBOX_PATH "/vifminfo.json");
}

TEST(partial_restart_preserves_tabs)
{
	histories_init(10);
	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", NULL);
	cfg.vifm_info = VINFO_DHISTORY | VINFO_TABS;

	setup_tabs();
	assert_success(cmds_dispatch("restart", &lwin, CIT_COMMAND));
	assert_int_equal(2, tabs_count(&lwin));
}

TEST(full_restart_drops_tabs)
{
	histories_init(10);
	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", NULL);
	cfg.vifm_info = VINFO_DHISTORY | VINFO_TABS;

	setup_tabs();
	assert_success(cmds_dispatch("restart full", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_count(&lwin));
}

static void
setup_tabs(void)
{
	flist_hist_setup(&lwin, "/t1ldir1", "t1lfile1", 1, 1);
	flist_hist_setup(&lwin, "/t1ldir2", "t1lfile2", 2, 1);
	flist_hist_setup(&lwin, "/path", "", 3, 1);
	flist_hist_setup(&rwin, "/t1rdir1", "t1rfile1", 1, 1);
	flist_hist_setup(&rwin, "/t1rdir2", "t1rfile2", 2, 1);
	flist_hist_setup(&rwin, "/path", "", 3, 1);

	assert_success(cmds_dispatch("tabnew", &lwin, CIT_COMMAND));

	flist_hist_setup(&lwin, "/t2ldir1", "t2lfile1", 1, 1);
	flist_hist_setup(&lwin, "/t2ldir2", "t2lfile2", 2, 1);
	flist_hist_setup(&lwin, "/path", "", 3, 1);
	flist_hist_setup(&rwin, "/t2rdir1", "t2rfile1", 1, 1);
	flist_hist_setup(&rwin, "/t2rdir2", "t2rfile2", 2, 1);
	flist_hist_setup(&rwin, "/path", "", 3, 1);
}

static void
check_first_tab(void)
{
	assert_int_equal(4, lwin.history_num);
	if(lwin.history_num >= 4)
	{
		assert_string_equal("/path", lwin.history[0].dir);
		assert_string_equal("", lwin.history[0].file);
		assert_string_equal("/t1ldir1", lwin.history[1].dir);
		assert_string_equal("t1lfile1", lwin.history[1].file);
		assert_string_equal("/t1ldir2", lwin.history[2].dir);
		assert_string_equal("t1lfile2", lwin.history[2].file);
		assert_string_equal("/path", lwin.history[3].dir);
		assert_string_equal("", lwin.history[3].file);
	}

	assert_int_equal(4, rwin.history_num);
	if(rwin.history_num >= 4)
	{
		assert_string_equal("/path", rwin.history[0].dir);
		assert_string_equal("", rwin.history[0].file);
		assert_string_equal("/t1rdir1", rwin.history[1].dir);
		assert_string_equal("t1rfile1", rwin.history[1].file);
		assert_string_equal("/t1rdir2", rwin.history[2].dir);
		assert_string_equal("t1rfile2", rwin.history[2].file);
		assert_string_equal("/path", rwin.history[3].dir);
		assert_string_equal("", rwin.history[3].file);
	}
}

static void
check_second_tab(void)
{
	assert_int_equal(7, lwin.history_num);
	if(lwin.history_num >= 7)
	{
		assert_string_equal("/path", lwin.history[0].dir);
		assert_string_equal("", lwin.history[0].file);
		assert_string_equal("/t1ldir1", lwin.history[1].dir);
		assert_string_equal("t1lfile1", lwin.history[1].file);
		assert_string_equal("/t1ldir2", lwin.history[2].dir);
		assert_string_equal("t1lfile2", lwin.history[2].file);
		assert_string_equal("/path", lwin.history[3].dir);
		assert_string_equal("", lwin.history[3].file);
		assert_string_equal("/t2ldir1", lwin.history[4].dir);
		assert_string_equal("t2lfile1", lwin.history[4].file);
		assert_string_equal("/t2ldir2", lwin.history[5].dir);
		assert_string_equal("t2lfile2", lwin.history[5].file);
		assert_string_equal("/path", lwin.history[6].dir);
		assert_string_equal("", lwin.history[6].file);
	}

	assert_int_equal(7, rwin.history_num);
	if(rwin.history_num >= 7)
	{
		assert_string_equal("/path", rwin.history[0].dir);
		assert_string_equal("", rwin.history[0].file);
		assert_string_equal("/t1rdir1", rwin.history[1].dir);
		assert_string_equal("t1rfile1", rwin.history[1].file);
		assert_string_equal("/t1rdir2", rwin.history[2].dir);
		assert_string_equal("t1rfile2", rwin.history[2].file);
		assert_string_equal("/path", rwin.history[3].dir);
		assert_string_equal("", rwin.history[3].file);
		assert_string_equal("/t2rdir1", rwin.history[4].dir);
		assert_string_equal("t2rfile1", rwin.history[4].file);
		assert_string_equal("/t2rdir2", rwin.history[5].dir);
		assert_string_equal("t2rfile2", rwin.history[5].file);
		assert_string_equal("/path", rwin.history[6].dir);
		assert_string_equal("", rwin.history[6].file);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
