#include <stic.h>

#include <stdio.h> /* rename() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/cfg/info.h"
#include "../../src/utils/hist.h"
#include "../../src/status.h"

SETUP_ONCE()
{
	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", NULL);
}

TEARDOWN_ONCE()
{
	cfg.config_dir[0] = '\0';
}

TEARDOWN()
{
	(void)sessions_stop();
	histories_init(0);
	cfg.session_options = 0;
	cfg.vifm_info = 0;
}

TEST(not_in_a_session_initially)
{
	assert_string_equal("", sessions_current());
}

TEST(can_create_new_session)
{
	assert_success(sessions_create("session"));
	assert_string_equal("session", sessions_current());
}

TEST(cannot_create_same_session_second_time)
{
	assert_success(sessions_create("session"));
	assert_string_equal("session", sessions_current());

	assert_failure(sessions_create("session"));
	assert_string_equal("session", sessions_current());
}

TEST(cannot_create_existing_session)
{
	create_dir(SANDBOX_PATH "/sessions");
	create_file(SANDBOX_PATH "/sessions/session.json");

	assert_failure(sessions_create("session"));

	remove_file(SANDBOX_PATH "/sessions/session.json");
	remove_dir(SANDBOX_PATH "/sessions");
}

TEST(cannot_stop_session_when_there_is_none)
{
	assert_string_equal("", sessions_current());
	assert_failure(sessions_stop());
	assert_string_equal("", sessions_current());
}

TEST(can_stop_session)
{
	assert_success(sessions_create("session"));
	assert_string_equal("session", sessions_current());

	assert_success(sessions_stop());
	assert_string_equal("", sessions_current());
}

TEST(session_is_store_as_part_of_state)
{
	create_dir(SANDBOX_PATH "/sessions");

	cfg.session_options = VINFO_CHISTORY;
	assert_success(sessions_create("session"));
	state_store();

	remove_file(SANDBOX_PATH "/sessions/session.json");
	remove_dir(SANDBOX_PATH "/sessions");
	remove_file(SANDBOX_PATH "/vifminfo.json");
}

TEST(session_is_not_stored_for_empty_session_options)
{
	assert_success(sessions_create("session"));
	state_store();

	no_remove_dir(SANDBOX_PATH "/sessions");
	remove_file(SANDBOX_PATH "/vifminfo.json");
}

TEST(session_directory_is_created_on_storing)
{
	cfg.session_options = VINFO_CHISTORY;
	assert_success(sessions_create("session"));
	state_store();

	remove_file(SANDBOX_PATH "/sessions/session.json");
	remove_dir(SANDBOX_PATH "/sessions");
	remove_file(SANDBOX_PATH "/vifminfo.json");
}

TEST(session_can_be_loaded)
{
	histories_init(10);
	cfg.session_options = VINFO_CHISTORY;
	assert_success(sessions_create("session"));

	hist_add(&curr_stats.cmd_hist, "command0", 0);
	hist_add(&curr_stats.cmd_hist, "command2", 2);

	state_store();

	histories_init(10);
	sessions_load("session");

	assert_int_equal(2, curr_stats.cmd_hist.size);

	assert_string_equal("command2", curr_stats.cmd_hist.items[0].text);
	assert_int_equal(2, curr_stats.cmd_hist.items[0].timestamp);
	assert_string_equal("command0", curr_stats.cmd_hist.items[1].text);
	assert_int_equal(0, curr_stats.cmd_hist.items[1].timestamp);

	remove_file(SANDBOX_PATH "/sessions/session.json");
	remove_dir(SANDBOX_PATH "/sessions");
	remove_file(SANDBOX_PATH "/vifminfo.json");

	histories_init(0);
}

TEST(loading_nonexisting_session_just_loads_vifminfo)
{
	histories_init(10);
	cfg.vifm_info = VINFO_CHISTORY;

	hist_add(&curr_stats.cmd_hist, "command0", 0);
	hist_add(&curr_stats.cmd_hist, "command2", 2);

	state_store();

	histories_init(10);
	sessions_load("session");

	assert_int_equal(2, curr_stats.cmd_hist.size);

	assert_string_equal("command2", curr_stats.cmd_hist.items[0].text);
	assert_int_equal(2, curr_stats.cmd_hist.items[0].timestamp);
	assert_string_equal("command0", curr_stats.cmd_hist.items[1].text);
	assert_int_equal(0, curr_stats.cmd_hist.items[1].timestamp);

	remove_file(SANDBOX_PATH "/vifminfo.json");

	histories_init(0);
}

TEST(session_is_merged_on_storing)
{
	histories_init(10);
	cfg.session_options = VINFO_CHISTORY;
	assert_success(sessions_create("session"));

	hist_add(&curr_stats.cmd_hist, "command0", 0);
	hist_add(&curr_stats.cmd_hist, "command2", 2);

	state_store();

	histories_init(10);
	hist_add(&curr_stats.cmd_hist, "command1", 1);
	hist_add(&curr_stats.cmd_hist, "command3", 3);

	/* Second time, touched session file, merging is necessary. */
	reset_timestamp(SANDBOX_PATH "/sessions/session.json");
	state_store();

	histories_init(10);
	sessions_load("session");

	assert_int_equal(4, curr_stats.cmd_hist.size);

	assert_string_equal("command3", curr_stats.cmd_hist.items[0].text);
	assert_int_equal(3, curr_stats.cmd_hist.items[0].timestamp);
	assert_string_equal("command2", curr_stats.cmd_hist.items[1].text);
	assert_int_equal(2, curr_stats.cmd_hist.items[1].timestamp);
	assert_string_equal("command1", curr_stats.cmd_hist.items[2].text);
	assert_int_equal(1, curr_stats.cmd_hist.items[2].timestamp);
	assert_string_equal("command0", curr_stats.cmd_hist.items[3].text);
	assert_int_equal(0, curr_stats.cmd_hist.items[3].timestamp);

	remove_file(SANDBOX_PATH "/sessions/session.json");
	remove_dir(SANDBOX_PATH "/sessions");
	remove_file(SANDBOX_PATH "/vifminfo.json");
}

TEST(session_is_merged_after_switching)
{
	histories_init(10);
	cfg.session_options = VINFO_CHISTORY;
	assert_success(sessions_create("session-a"));

	hist_add(&curr_stats.cmd_hist, "command0", 0);
	hist_add(&curr_stats.cmd_hist, "command2", 2);

	state_store();
	assert_success(sessions_create("session-b"));

	assert_success(rename(SANDBOX_PATH "/sessions/session-a.json",
			SANDBOX_PATH "/sessions/session-b.json"));

	histories_init(10);
	hist_add(&curr_stats.cmd_hist, "command1", 1);
	hist_add(&curr_stats.cmd_hist, "command3", 3);

	state_store();

	histories_init(10);
	sessions_load("session-b");

	assert_int_equal(4, curr_stats.cmd_hist.size);

	remove_file(SANDBOX_PATH "/sessions/session-b.json");
	remove_dir(SANDBOX_PATH "/sessions");
	remove_file(SANDBOX_PATH "/vifminfo.json");
}

TEST(can_check_for_existing_session)
{
	create_dir(SANDBOX_PATH "/sessions");
	create_file(SANDBOX_PATH "/sessions/session.json");

	assert_true(sessions_exists("session"));

	remove_file(SANDBOX_PATH "/sessions/session.json");
	remove_dir(SANDBOX_PATH "/sessions");
}

TEST(can_check_for_nonexisting_session)
{
	create_dir(SANDBOX_PATH "/sessions");

	assert_false(sessions_exists("session"));

	remove_dir(SANDBOX_PATH "/sessions");
}

TEST(directories_are_not_sessions)
{
	create_dir(SANDBOX_PATH "/sessions");
	create_dir(SANDBOX_PATH "/sessions/session.json");

	assert_false(sessions_exists("session"));

	remove_dir(SANDBOX_PATH "/sessions/session.json");
	remove_dir(SANDBOX_PATH "/sessions");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
