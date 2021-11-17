#include <stic.h>

#include <sys/stat.h> /* chmod() */

#include <stdlib.h> /* free() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/autocmds.h"
#include "../../src/engine/keys.h"
#include "../../src/engine/variables.h"
#include "../../src/ui/column_view.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/tabs.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/env.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/status.h"

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
	view_setup(&lwin);
	view_setup(&rwin);

	lwin.columns = columns_create();
	rwin.columns = columns_create();
	columns_setup_column(SK_BY_NAME);
	columns_setup_column(SK_BY_SIZE);

	init_commands();
	opt_handlers_setup();
}

TEARDOWN()
{
	(void)exec_commands("session", &lwin, CIT_COMMAND);

	cfg.config_dir[0] = '\0';
	cfg.session_options = 0;

	opt_handlers_teardown();
	vle_keys_reset();

	view_teardown(&lwin);
	view_teardown(&rwin);

	columns_free(lwin.columns);
	lwin.columns = NULL;
	columns_free(rwin.columns);
	rwin.columns = NULL;
	columns_teardown();
}

TEST(can_create_a_session)
{
	ui_sb_msg("");
	assert_failure(exec_commands("session sess", &lwin, CIT_COMMAND));
	assert_string_equal("Switched to a new session: sess", ui_sb_last());
}

TEST(query_no_session)
{
	ui_sb_msg("");
	assert_failure(exec_commands("session?", &lwin, CIT_COMMAND));
	assert_string_equal("No active session", ui_sb_last());
}

TEST(query_current_session)
{
	assert_failure(exec_commands("session sess", &lwin, CIT_COMMAND));

	ui_sb_msg("");
	assert_failure(exec_commands("session?", &lwin, CIT_COMMAND));
	assert_string_equal("Active session: sess", ui_sb_last());
}

TEST(detach_no_session)
{
	ui_sb_msg("");
	assert_failure(exec_commands("session", &lwin, CIT_COMMAND));
	assert_string_equal("No active session", ui_sb_last());
}

TEST(detach_current_session)
{
	assert_failure(exec_commands("session sess", &lwin, CIT_COMMAND));

	ui_sb_msg("");
	assert_failure(exec_commands("session", &lwin, CIT_COMMAND));
	assert_string_equal("Detached from session without saving: sess",
			ui_sb_last());
}

TEST(reject_name_with_slash)
{
	ui_sb_msg("");
	assert_failure(exec_commands("session a/b", &lwin, CIT_COMMAND));
	assert_string_equal("Session name can't include path separators",
			ui_sb_last());
}

TEST(reject_to_restart_current_session)
{
	assert_failure(exec_commands("session sess", &lwin, CIT_COMMAND));
	ui_sb_msg("");
	assert_failure(exec_commands("session sess", &lwin, CIT_COMMAND));
	assert_string_equal("Already active session: sess", ui_sb_last());
}

TEST(store_previous_session_before_switching)
{
	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", NULL);
	cfg.session_options = VINFO_CHISTORY;

	assert_failure(exec_commands("session first", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("session second", &lwin, CIT_COMMAND));

	remove_file(SANDBOX_PATH "/sessions/first.json");
	remove_dir(SANDBOX_PATH "/sessions");
	remove_file(SANDBOX_PATH "/vifminfo.json");
}

TEST(session_is_stored_with_general_state)
{
	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", NULL);
	cfg.session_options = VINFO_CHISTORY;

	assert_failure(exec_commands("session session", &lwin, CIT_COMMAND));
	assert_success(exec_commands("write", &lwin, CIT_COMMAND));

	remove_file(SANDBOX_PATH "/sessions/session.json");
	remove_dir(SANDBOX_PATH "/sessions");
	remove_file(SANDBOX_PATH "/vifminfo.json");
}

TEST(can_load_a_session)
{
	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", NULL);
	cfg.session_options = VINFO_CHISTORY;

	histories_init(10);
	hist_add(&curr_stats.cmd_hist, "command1", 1);

	assert_failure(exec_commands("session sess", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("session other", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("session", &lwin, CIT_COMMAND));

	histories_init(10);

	ui_sb_msg("");
	assert_failure(exec_commands("session sess", &lwin, CIT_COMMAND));
	assert_string_equal("Loaded session: sess", ui_sb_last());

	assert_int_equal(1, curr_stats.cmd_hist.size);

	remove_file(SANDBOX_PATH "/sessions/sess.json");
	remove_dir(SANDBOX_PATH "/sessions");
	remove_file(SANDBOX_PATH "/vifminfo.json");

	histories_init(0);
}

TEST(can_delete_a_session)
{
	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", NULL);

	ui_sb_msg("");
	assert_failure(exec_commands("delsession sess", &lwin, CIT_COMMAND));
	assert_string_equal("No stored sessions with such name: sess", ui_sb_last());

	create_dir(SANDBOX_PATH "/sessions");
	create_dir(SANDBOX_PATH "/sessions/not-a-session.json");
	create_file(SANDBOX_PATH "/sessions/session.json");

	if(regular_unix_user())
	{
		assert_success(chmod(SANDBOX_PATH "/sessions", 0555));
		ui_sb_msg("");
		assert_failure(exec_commands("delsession session", &lwin, CIT_COMMAND));
		assert_string_equal("Failed to delete a session: session", ui_sb_last());
		assert_success(chmod(SANDBOX_PATH "/sessions", 0777));
	}

	assert_success(exec_commands("delsession session", &lwin, CIT_COMMAND));

	no_remove_file(SANDBOX_PATH "/sessions/session.json");
	remove_dir(SANDBOX_PATH "/sessions/not-a-session.json");
	remove_dir(SANDBOX_PATH "/sessions");
}

TEST(can_fail_to_switch_and_still_be_in_a_session)
{
	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", NULL);
	curr_stats.load_stage = -1;
	env_set("MYVIFMRC", SANDBOX_PATH "/vifmrc");

	create_dir(SANDBOX_PATH "/sessions");
	create_file(SANDBOX_PATH "/sessions/empty.json");
	make_file(SANDBOX_PATH "/vifmrc", "session bla");

	ui_sb_msg("");
	assert_failure(exec_commands("session empty", &lwin, CIT_COMMAND));
	assert_string_equal("Session switching has failed, active session: bla",
			ui_sb_last());
	char *value = var_to_str(getvar("v:session"));
	assert_string_equal("bla", value);
	free(value);

	remove_file(SANDBOX_PATH "/vifmrc");
	remove_file(SANDBOX_PATH "/sessions/empty.json");
	remove_dir(SANDBOX_PATH "/sessions");

	curr_stats.load_stage = 0;
	env_remove("MYVIFMRC");
}

TEST(load_previous_session)
{
	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", NULL);
	curr_stats.load_stage = -1;
	env_set("MYVIFMRC", SANDBOX_PATH "/vifmrc");
	cfg.session_options = VINFO_CHISTORY;

	/* No previous session. */
	update_string(&curr_stats.last_session, NULL);
	ui_sb_msg("");
	assert_failure(exec_commands("session -", &lwin, CIT_COMMAND));
	assert_string_equal("No previous session", ui_sb_last());

	/* Detached session. */
	assert_failure(exec_commands("session tmp", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("session", &lwin, CIT_COMMAND));
	ui_sb_msg("");
	assert_failure(exec_commands("session -", &lwin, CIT_COMMAND));
	assert_string_equal("Previous session doesn't exist", ui_sb_last());

	/* Previous session. */
	assert_failure(exec_commands("session first", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("session second", &lwin, CIT_COMMAND));
	ui_sb_msg("");
	assert_failure(exec_commands("session -", &lwin, CIT_COMMAND));
	assert_string_equal("Loaded session: first", ui_sb_last());

	remove_file(SANDBOX_PATH "/sessions/first.json");
	remove_file(SANDBOX_PATH "/sessions/second.json");
	remove_dir(SANDBOX_PATH "/sessions");

	curr_stats.load_stage = 0;
	env_remove("MYVIFMRC");
}

TEST(vsession_is_empty_initially)
{
	char *value = var_to_str(getvar("v:session"));
	assert_string_equal("", value);
	free(value);
}

TEST(vsession_is_set)
{
	assert_failure(exec_commands("session sess", &lwin, CIT_COMMAND));
	char *value = var_to_str(getvar("v:session"));
	assert_string_equal("sess", value);
	free(value);
}

TEST(vsession_is_empty_after_detaching)
{
	assert_failure(exec_commands("session sess", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("session", &lwin, CIT_COMMAND));
	char *value = var_to_str(getvar("v:session"));
	assert_string_equal("", value);
	free(value);
}

TEST(vsession_is_emptied_on_failure_to_load_a_session)
{
	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", NULL);
	cfg.session_options = VINFO_CHISTORY;

	create_dir(SANDBOX_PATH "/sessions");
	create_file(SANDBOX_PATH "/sessions/empty.json");

	assert_failure(exec_commands("session sess", &lwin, CIT_COMMAND));
	ui_sb_msg("");
	assert_failure(exec_commands("session empty", &lwin, CIT_COMMAND));
	assert_string_equal("Session switching has failed, no active session",
			ui_sb_last());
	char *value = var_to_str(getvar("v:session"));
	assert_string_equal("", value);
	free(value);

	remove_file(SANDBOX_PATH "/sessions/empty.json");
	remove_file(SANDBOX_PATH "/sessions/sess.json");
	remove_dir(SANDBOX_PATH "/sessions");
	remove_file(SANDBOX_PATH "/vifminfo.json");
}

TEST(autocmd_is_called_for_all_global_tabs)
{
	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", NULL);
	make_file(SANDBOX_PATH "/vifmrc", "autocmd DirEnter * setl dotfiles");
	cfg.session_options = VINFO_TABS | VINFO_DHISTORY | VINFO_SAVEDIRS;

	curr_stats.load_stage = -1;

	setup_grid(&lwin, 1, 1, 1);
	setup_grid(&rwin, 1, 1, 1);

	create_dir(SANDBOX_PATH "/sessions");

	assert_failure(exec_commands("session sess", &lwin, CIT_COMMAND));
	assert_success(exec_commands("tabnew", &lwin, CIT_COMMAND));
	assert_success(exec_commands("write", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("session", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("session sess", &lwin, CIT_COMMAND));
	assert_success(exec_commands("tabnext", &lwin, CIT_COMMAND));

	int i;
	tab_info_t tab_info;
	for(i = 0; tabs_enum_all(i, &tab_info); ++i)
	{
		assert_false(tab_info.view->hide_dot);
	}

	remove_file(SANDBOX_PATH "/sessions/sess.json");
	remove_dir(SANDBOX_PATH "/sessions");
	remove_file(SANDBOX_PATH "/vifminfo.json");
	remove_file(SANDBOX_PATH "/vifmrc");

	tabs_only(&lwin);
	tabs_only(&rwin);

	vle_aucmd_remove(NULL, NULL);
	curr_stats.load_stage = 0;
}

TEST(autocmd_is_called_for_all_pane_tabs)
{
	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", NULL);
	make_file(SANDBOX_PATH "/vifmrc", "autocmd DirEnter * setl dotfiles");
	cfg.session_options = VINFO_TABS | VINFO_DHISTORY | VINFO_SAVEDIRS;

	cfg.pane_tabs = 1;
	curr_stats.load_stage = -1;

	setup_grid(&lwin, 1, 1, 1);
	setup_grid(&rwin, 1, 1, 1);

	create_dir(SANDBOX_PATH "/sessions");

	assert_failure(exec_commands("session sess", &lwin, CIT_COMMAND));
	assert_success(exec_commands("tabnew", &lwin, CIT_COMMAND));
	assert_success(exec_commands("write", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("session", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("session sess", &lwin, CIT_COMMAND));
	assert_success(exec_commands("tabnext", &lwin, CIT_COMMAND));

	int i;
	tab_info_t tab_info;
	for(i = 0; tabs_enum_all(i, &tab_info); ++i)
	{
		assert_false(tab_info.view->hide_dot);
	}

	remove_file(SANDBOX_PATH "/sessions/sess.json");
	remove_dir(SANDBOX_PATH "/sessions");
	remove_file(SANDBOX_PATH "/vifminfo.json");
	remove_file(SANDBOX_PATH "/vifmrc");

	tabs_only(&lwin);
	tabs_only(&rwin);

	vle_aucmd_remove(NULL, NULL);
	curr_stats.load_stage = 0;
	cfg.pane_tabs = 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
