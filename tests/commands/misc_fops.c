#include <stic.h>

#include <unistd.h> /* chdir() rmdir() unlink() */

#include <limits.h> /* INT_MAX */
#include <stdio.h> /* remove() snprintf() */
#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/registers.h"

static char *saved_cwd;

static char cwd[PATH_MAX + 1];
static char sandbox[PATH_MAX + 1];

SETUP_ONCE()
{
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
	make_abs_path(sandbox, sizeof(sandbox), SANDBOX_PATH, "", cwd);
}

SETUP()
{
	view_setup(&lwin);
	view_setup(&rwin);

	curr_view = &lwin;
	other_view = &rwin;

	conf_setup();
	undo_setup();
	cmds_init();

	saved_cwd = save_cwd();
}

TEARDOWN()
{
	restore_cwd(saved_cwd);

	view_teardown(&lwin);
	view_teardown(&rwin);

	conf_teardown();
	vle_cmds_reset();
	undo_teardown();
}

TEST(tr_extends_second_field)
{
	char path[PATH_MAX + 1];

	assert_success(chdir(sandbox));

	strcpy(lwin.curr_dir, sandbox);
	append_view_entry(&lwin, "a b");

	snprintf(path, sizeof(path), "%s/a b", sandbox);
	create_file(path);

	(void)cmds_dispatch("tr/ ?<>\\\\:*|\"/_", &lwin, CIT_COMMAND);

	snprintf(path, sizeof(path), "%s/a_b", sandbox);
	assert_success(remove(path));
}

TEST(substitute_works)
{
	char path[PATH_MAX + 1];

	assert_success(chdir(sandbox));

	strcpy(lwin.curr_dir, sandbox);
	append_view_entry(&lwin, "a b b");
	append_view_entry(&lwin, "B c");

	snprintf(path, sizeof(path), "%s/a b b", sandbox);
	create_file(path);
	snprintf(path, sizeof(path), "%s/B c", sandbox);
	create_file(path);

	(void)cmds_dispatch("%substitute/b/c/Iig", &lwin, CIT_COMMAND);

	snprintf(path, sizeof(path), "%s/a c c", sandbox);
	assert_success(remove(path));
	snprintf(path, sizeof(path), "%s/c c", sandbox);
	assert_success(remove(path));
}

TEST(chmod_works, IF(not_windows))
{
	char path[PATH_MAX + 1];

	assert_success(chdir(sandbox));

	strcpy(lwin.curr_dir, sandbox);
	append_view_entry(&lwin, "file1");
	append_view_entry(&lwin, "file2");

	snprintf(path, sizeof(path), "%s/file1", sandbox);
	create_file(path);
	snprintf(path, sizeof(path), "%s/file2", sandbox);
	create_file(path);

	(void)cmds_dispatch("1,2chmod +x", &lwin, CIT_COMMAND);

	populate_dir_list(&lwin, 1);
	assert_int_equal(FT_EXEC, lwin.dir_entry[0].type);
	assert_int_equal(FT_EXEC, lwin.dir_entry[1].type);

	snprintf(path, sizeof(path), "%s/file1", sandbox);
	assert_success(remove(path));
	snprintf(path, sizeof(path), "%s/file2", sandbox);
	assert_success(remove(path));
}

TEST(putting_files_works)
{
	char path[PATH_MAX + 1];

	regs_init();

	assert_success(os_mkdir(SANDBOX_PATH "/empty-dir", 0700));
	assert_success(flist_load_tree(&lwin, sandbox, INT_MAX));

	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "read/binary-data", cwd);
	assert_success(regs_append(DEFAULT_REG_NAME, path));
	lwin.list_pos = 1;

	assert_true(cmds_dispatch("put", &lwin, CIT_COMMAND) != 0);
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(unlink(SANDBOX_PATH "/empty-dir/binary-data"));
	assert_success(rmdir(SANDBOX_PATH "/empty-dir"));

	regs_reset();
}

TEST(yank_works_with_ranges)
{
	char path[PATH_MAX + 1];
	const reg_t *reg;

	regs_init();

	flist_custom_start(&lwin, "test");
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "existing-files/a", cwd);
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	reg = regs_find(DEFAULT_REG_NAME);
	assert_non_null(reg);

	assert_int_equal(0, reg->nfiles);
	(void)cmds_dispatch("%yank", &lwin, CIT_COMMAND);
	assert_int_equal(1, reg->nfiles);

	regs_reset();
}

TEST(touch)
{
	to_canonic_path(SANDBOX_PATH, cwd, lwin.curr_dir, sizeof(lwin.curr_dir));
	(void)cmds_dispatch("touch file", &lwin, CIT_COMMAND);

	assert_success(remove(SANDBOX_PATH "/file"));
}

TEST(zero_count_is_rejected)
{
	const char *expected = "Count argument can't be zero";

	ui_sb_msg("");
	assert_failure(cmds_dispatch("delete a 0", &lwin, CIT_COMMAND));
	assert_string_equal(expected, ui_sb_last());

	ui_sb_msg("");
	assert_failure(cmds_dispatch("yank a 0", &lwin, CIT_COMMAND));
	assert_string_equal(expected, ui_sb_last());
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
