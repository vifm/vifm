#include <stic.h>

#include <sys/stat.h> /* stat */
#include <unistd.h> /* stat() rmdir() unlink() */

#include <string.h> /* strcpy() */

#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/filelist.h"
#include "../../src/fileops.h"
#include "../../src/registers.h"
#include "../../src/trash.h"

#include "utils.h"

static void line_prompt(const char prompt[], const char filename[],
		fo_prompt_cb cb, fo_complete_cmd_func complete, int allow_ee);
static char options_prompt_rename(const char title[], const char message[],
		const struct response_variant *variants);
static char options_prompt_overwrite(const char title[], const char message[],
		const struct response_variant *variants);

SETUP()
{
	regs_init();

	if(is_path_absolute(SANDBOX_PATH))
	{
		strcpy(lwin.curr_dir, SANDBOX_PATH);
	}
	else
	{
		char cwd[PATH_MAX];
		assert_non_null(get_cwd(cwd, sizeof(cwd)));
		snprintf(lwin.curr_dir, sizeof(lwin.curr_dir), "%s/%s", cwd, SANDBOX_PATH);
	}
}

TEARDOWN()
{
	regs_reset();
}

static void
line_prompt(const char prompt[], const char filename[], fo_prompt_cb cb,
		fo_complete_cmd_func complete, int allow_ee)
{
	cb("b");
}

static char
options_prompt_rename(const char title[], const char message[],
		const struct response_variant *variants)
{
	init_fileops(&line_prompt, &options_prompt_overwrite);
	return 'r';
}

static char
options_prompt_overwrite(const char title[], const char message[],
		const struct response_variant *variants)
{
	return 'o';
}

TEST(put_files_bg_fails_on_wrong_register)
{
	assert_true(put_files_bg(&lwin, -1, -1, 0));
	wait_for_bg();
}

TEST(put_files_bg_fails_on_empty_register)
{
	assert_true(put_files_bg(&lwin, -1, 'a', 0));
	wait_for_bg();
}

TEST(put_files_bg_fails_on_identical_names_in_a_register)
{
	assert_success(regs_append('a', TEST_DATA_PATH "/existing-files/a"));
	assert_success(regs_append('a', TEST_DATA_PATH "/rename/a"));

	assert_true(put_files_bg(&lwin, -1, 'a', 0));
	wait_for_bg();
}

TEST(put_files_bg_fails_on_file_name_conflict)
{
	create_empty_file(SANDBOX_PATH "/a");

	assert_success(regs_append('a', TEST_DATA_PATH "/rename/a"));

	assert_true(put_files_bg(&lwin, -1, 'a', 0));
	wait_for_bg();

	assert_success(unlink(SANDBOX_PATH "/a"));
}

TEST(put_files_bg_copies_files)
{
	assert_success(regs_append('a', TEST_DATA_PATH "/existing-files/a"));

	assert_int_equal(0, put_files_bg(&lwin, -1, 'a', 0));
	wait_for_bg();

	assert_success(unlink(SANDBOX_PATH "/a"));
}

TEST(put_files_bg_skips_nonexistent_source_files)
{
	create_empty_dir(SANDBOX_PATH "/dir");
	create_empty_file(SANDBOX_PATH "/dir/b");

	assert_success(regs_append('a', TEST_DATA_PATH "/existing-files/a"));
	assert_success(regs_append('a', SANDBOX_PATH "/dir/b"));
	assert_success(unlink(SANDBOX_PATH "/dir/b"));

	assert_int_equal(0, put_files_bg(&lwin, -1, 'a', 0));
	wait_for_bg();

	assert_success(unlink(SANDBOX_PATH "/a"));
	assert_success(rmdir(SANDBOX_PATH "/dir"));
}

TEST(put_files_bg_demangles_names_of_trashed_files)
{
	set_trash_dir(SANDBOX_PATH "/trash");

	create_empty_dir(SANDBOX_PATH "/trash");
	create_empty_file(SANDBOX_PATH "/trash/000_b");

	assert_success(regs_append('a', SANDBOX_PATH "/trash/000_b"));

	assert_int_equal(0, put_files_bg(&lwin, -1, 'a', 1));
	wait_for_bg();

	assert_success(unlink(SANDBOX_PATH "/b"));
	assert_success(rmdir(SANDBOX_PATH "/trash"));
}

TEST(put_files_copies_files_according_to_tree_structure)
{
	char *const saved_cwd = save_cwd();

	view_setup(&lwin);

	create_empty_dir(SANDBOX_PATH "/dir");

	flist_load_tree(&lwin, SANDBOX_PATH);

	assert_success(regs_append('a', TEST_DATA_PATH "/existing-files/a"));

	/* Copy at the top level.  Set at to -1. */

	lwin.list_pos = 0;
	(void)put_files(&lwin, -1, 'a', 0);
	assert_success(unlink(SANDBOX_PATH "/a"));

	lwin.list_pos = 0;
	assert_int_equal(0, put_files_bg(&lwin, -1, 'a', 0));
	wait_for_bg();
	assert_success(unlink(SANDBOX_PATH "/a"));

	/* Copy at nested level.  Set at to desired position. */

	(void)put_files(&lwin, 1, 'a', 0);
	assert_success(unlink(SANDBOX_PATH "/dir/a"));

	/* Here target position in 100, which should become 1 automatically. */
	assert_int_equal(0, put_files_bg(&lwin, 100, 'a', 0));
	wait_for_bg();
	assert_success(unlink(SANDBOX_PATH "/dir/a"));

	restore_cwd(saved_cwd);
	assert_success(rmdir(SANDBOX_PATH "/dir"));

	view_teardown(&lwin);
}

TEST(overwrite_request_accounts_for_target_file_rename)
{
	struct stat st;
	char src_file[PATH_MAX];
	FILE *f;
	char *saved_cwd;

	f = fopen(SANDBOX_PATH "/binary-data", "w");
	fclose(f);

	f = fopen(SANDBOX_PATH "/b", "w");
	fclose(f);

	if(is_path_absolute(TEST_DATA_PATH))
	{
		snprintf(src_file, sizeof(src_file), "%s/read/binary-data", TEST_DATA_PATH);
	}
	else
	{
		char cwd[PATH_MAX];
		assert_non_null(get_cwd(cwd, sizeof(cwd)));
		snprintf(src_file, sizeof(src_file), "%s/%s/read/binary-data", cwd,
				TEST_DATA_PATH);
	}

	assert_success(regs_append('a', src_file));

	init_fileops(&line_prompt, &options_prompt_rename);

	saved_cwd = save_cwd();
	(void)put_files(&lwin, -1, 'a', 0);
	restore_cwd(saved_cwd);

	assert_success(stat(SANDBOX_PATH "/binary-data", &st));
	assert_int_equal(0, st.st_size);

	assert_success(stat(SANDBOX_PATH "/b", &st));
	assert_int_equal(1024, st.st_size);

	(void)remove(SANDBOX_PATH "/binary-data");
	(void)remove(SANDBOX_PATH "/b");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
