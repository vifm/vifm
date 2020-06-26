#include <stic.h>

#include <sys/stat.h> /* chmod() */

#include <stdlib.h> /* free() */

#include <stubs.h>
#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/variables.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/env.h"
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
	init_commands();
}

TEARDOWN()
{
	(void)exec_commands("session", &lwin, CIT_COMMAND);

	cfg.config_dir[0] = '\0';
	cfg.session_options = 0;
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

#ifndef _WIN32
	assert_success(chmod(SANDBOX_PATH "/sessions", 0555));
	ui_sb_msg("");
	assert_failure(exec_commands("delsession session", &lwin, CIT_COMMAND));
	assert_string_equal("Failed to delete a session: session", ui_sb_last());
	assert_success(chmod(SANDBOX_PATH "/sessions", 0777));
#endif

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
	vifm_tests_finish_restart_hook = &cfg_load;

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

	vifm_tests_finish_restart_hook = NULL;
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
