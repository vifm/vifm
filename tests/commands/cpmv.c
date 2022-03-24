#include <stic.h>

#include <unistd.h> /* chdir() */

#include <string.h> /* strcpy() strdup() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"

static char cwd[PATH_MAX + 1];

SETUP_ONCE()
{
	curr_view = &lwin;
	other_view = &rwin;

	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	undo_setup();
	conf_setup();
	cfg.use_system_calls = 1;
}

TEARDOWN_ONCE()
{
	undo_teardown();
	conf_teardown();
	cfg.use_system_calls = 0;
}

SETUP()
{
	view_setup(&lwin);
	view_setup(&rwin);

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "left",
			cwd);
	make_abs_path(rwin.curr_dir, sizeof(rwin.curr_dir), SANDBOX_PATH, "right",
			cwd);

	init_commands();

	create_dir(SANDBOX_PATH "/left");
	make_file(SANDBOX_PATH "/left/a", "1");
	make_file(SANDBOX_PATH "/left/b", "1");

	create_dir(SANDBOX_PATH "/right");
	make_file(SANDBOX_PATH "/right/a", "12");

	assert_success(populate_dir_list(&lwin, /*reload=*/0));
}

TEARDOWN()
{
	view_teardown(&lwin);
	view_teardown(&rwin);

	vle_cmds_reset();

	remove_file(SANDBOX_PATH "/left/a");
	remove_file(SANDBOX_PATH "/left/b");
	remove_dir(SANDBOX_PATH "/left");

	remove_file(SANDBOX_PATH "/right/a");
	remove_dir(SANDBOX_PATH "/right");
}

TEST(wrong_cpmv_flag)
{
	ui_sb_msg("");
	assert_failure(exec_commands("copy -wrong", &lwin, CIT_COMMAND));
	assert_string_equal("Unrecognized :command option: -wrong", ui_sb_last());

	ui_sb_msg("");
	assert_failure(exec_commands("alink -wrong", &lwin, CIT_COMMAND));
	assert_string_equal("Unrecognized :command option: -wrong", ui_sb_last());
}

TEST(copy_can_skip_existing_files)
{
	ui_sb_msg("");
	assert_failure(exec_commands("%copy -skip", &lwin, CIT_COMMAND));
	assert_string_equal("2 files successfully processed", ui_sb_last());

	assert_int_equal(2, get_file_size(SANDBOX_PATH "/right/a"));
	remove_file(SANDBOX_PATH "/right/b");
}

TEST(link_can_skip_existing_files, IF(not_windows))
{
	ui_sb_msg("");
	assert_failure(exec_commands("%alink -skip", &lwin, CIT_COMMAND));
	assert_string_equal("2 files successfully processed", ui_sb_last());

	assert_int_equal(2, get_file_size(SANDBOX_PATH "/right/a"));
	remove_file(SANDBOX_PATH "/right/b");
}

TEST(file_name_can_start_with_a_dash)
{
	ui_sb_msg("");
	assert_failure(exec_commands("%copy -- -test -skip", &lwin, CIT_COMMAND));
	assert_string_equal("2 files successfully processed", ui_sb_last());

	remove_file(SANDBOX_PATH "/right/-test");
	remove_file(SANDBOX_PATH "/right/-skip");
}

TEST(cpmv_does_not_crash_on_wrong_list_access)
{
	char path[PATH_MAX + 1];
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "existing-files", cwd);

	char sandbox[PATH_MAX + 1];
	make_abs_path(sandbox, sizeof(sandbox), SANDBOX_PATH, "", cwd);

	char *saved_cwd = save_cwd();
	assert_success(chdir(path));

	strcpy(lwin.curr_dir, path);
	strcpy(rwin.curr_dir, sandbox);

	free_dir_entries(&lwin, &lwin.dir_entry, &lwin.list_rows);

	lwin.list_rows = 3;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("a");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].name = strdup("b");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].selected = 1;
	lwin.dir_entry[2].name = strdup("c");
	lwin.dir_entry[2].origin = &lwin.curr_dir[0];
	lwin.dir_entry[2].selected = 1;
	lwin.selected_files = 3;

	/* cpmv used to use presence of the argument as indication of availability of
	 * file list and access memory beyond array boundaries. */
	(void)exec_commands("co .", &lwin, CIT_COMMAND);

	snprintf(path, sizeof(path), "%s/a", sandbox);
	assert_success(remove(path));
	snprintf(path, sizeof(path), "%s/b", sandbox);
	assert_success(remove(path));
	snprintf(path, sizeof(path), "%s/c", sandbox);
	assert_success(remove(path));

	restore_cwd(saved_cwd);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
