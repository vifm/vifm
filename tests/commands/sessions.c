#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
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

	cfg.config_dir[0] = '\0';
	cfg.session_options = 0;
}

SETUP()
{
	init_commands();
}

TEARDOWN()
{
	(void)exec_commands("session", &lwin, CIT_COMMAND);
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
