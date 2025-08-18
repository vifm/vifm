#include <stic.h>

#include <stdio.h> /* remove() */
#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/compat/os.h"
#include "../../src/cfg/config.h"
#include "../../src/modes/normal.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/status.h"

SETUP()
{
	curr_stats.save_msg = 0;
	view_setup(&lwin);
}

TEARDOWN()
{
	view_teardown(&lwin);
}

TEST(incorrect_length_causes_error, IF(not_windows))
{
	key_info_t key_info = { .count = 77777 };
	modnorm_cp(&lwin, key_info);
	assert_true(curr_stats.save_msg);
}

TEST(incorrect_num_causes_error, IF(not_windows))
{
	key_info_t key_info = { .count = 9 };
	modnorm_cp(&lwin, key_info);
	assert_true(curr_stats.save_msg);
}

TEST(file_mode_is_changed, IF(not_windows))
{
	struct stat st;

	key_info_t key_info = { .count = 777 };

	create_file(SANDBOX_PATH "/empty");
	assert_success(chmod(SANDBOX_PATH "/empty", 0000));

	strcpy(lwin.curr_dir, SANDBOX_PATH);
	append_view_entry(&lwin, "empty");

	replace_string(&cfg.shell, "/bin/sh");
	update_string(&cfg.shell_cmd_flag, "-c");
	stats_update_shell_type("/bin/sh");

	undo_setup();

	modnorm_cp(&lwin, key_info);
	assert_false(curr_stats.save_msg);

	assert_success(os_stat(SANDBOX_PATH "/empty", &st));
	assert_true((st.st_mode & 0777) != 0000);

	assert_success(remove(SANDBOX_PATH "/empty"));

	undo_teardown();

	update_string(&cfg.shell, NULL);
	update_string(&cfg.shell_cmd_flag, NULL);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
