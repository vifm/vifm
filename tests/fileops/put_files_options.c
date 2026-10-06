#include <stic.h>

#include <unistd.h> /* stat() rmdir() unlink() */

#include <test-utils.h>

#include "../../src/modes/dialogs/msg_dialog.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/fops_common.h"
#include "../../src/fops_put.h"
#include "../../src/registers.h"

/* The tests here are meant to check the set of options presented on a file
 * conflict. */

static void line_prompt(const char prompt[], const char filename[],
		fo_prompt_cb cb, void *cb_arg, fo_complete_cmd_func complete);
static char options_prompt_abort_and_record(const custom_prompt_t *details);

static char response_options[32];

static char *saved_cwd;

SETUP()
{
	saved_cwd = save_cwd();

	regs_init();
	fops_init(&line_prompt, &options_prompt_abort_and_record);

	view_setup(&lwin);
	lwin.sort[0] = SK_BY_NAME;
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "",
			saved_cwd);
	curr_view = NULL;
}

TEARDOWN()
{
	view_teardown(&lwin);
	regs_reset();
	restore_cwd(saved_cwd);
	fops_init(NULL, NULL);
}

TEST(show_merge_all_option_if_paths_include_dir)
{
	char path[PATH_MAX + 1];

	create_file(SANDBOX_PATH "/a");
	create_dir(SANDBOX_PATH "/dir");
	create_file(SANDBOX_PATH "/dir/a");
	create_file(SANDBOX_PATH "/dir/b");
	create_dir(SANDBOX_PATH "/dir/sub");

	make_abs_path(path, sizeof(path), SANDBOX_PATH, "/dir/a", saved_cwd);
	assert_success(regs_append('a', path));
	make_abs_path(path, sizeof(path), SANDBOX_PATH, "/dir/b", saved_cwd);
	assert_success(regs_append('a', path));

	response_options[0] = '\0';
	(void)fops_put(&lwin, -1, 'a', 0, /*deep=*/0);
	assert_string_doesnt_contain("M", response_options);

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	make_abs_path(path, sizeof(path), SANDBOX_PATH, "/dir/sub", saved_cwd);
	assert_success(regs_append('a', path));

	response_options[0] = '\0';
	(void)fops_put(&lwin, -1, 'a', 0, /*deep=*/0);
	assert_string_contains("M", response_options);

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(unlink(SANDBOX_PATH "/dir/a"));
	assert_success(unlink(SANDBOX_PATH "/dir/b"));
	assert_success(rmdir(SANDBOX_PATH "/dir/sub"));
	assert_success(rmdir(SANDBOX_PATH "/dir"));
	assert_success(unlink(SANDBOX_PATH "/a"));
	/* This one doesn't always get copied. */
	(void)unlink(SANDBOX_PATH "/b");
}

TEST(no_append_option_on_putting_file_links)
{
	create_file(SANDBOX_PATH "/dos-eof");

	char path[PATH_MAX + 1];
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "/read/dos-eof", saved_cwd);
	assert_success(regs_append('a', path));

	response_options[0] = '\0';
	(void)fops_put_links(&lwin, 'a', 0);
	assert_string_doesnt_contain("a", response_options);
	/* These are for directories, but won't hurt to check. */
	assert_string_doesnt_contain("m", response_options);
	assert_string_doesnt_contain("M", response_options);

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	remove_file(SANDBOX_PATH "/dos-eof");
}

TEST(no_merge_options_on_putting_dir_links)
{
	char path[PATH_MAX + 1];

	create_dir(SANDBOX_PATH "/sub");
	create_dir(SANDBOX_PATH "/dir");
	create_dir(SANDBOX_PATH "/dir/sub");

	make_abs_path(path, sizeof(path), SANDBOX_PATH, "/dir/sub", saved_cwd);
	assert_success(regs_append('a', path));

	response_options[0] = '\0';
	(void)fops_put_links(&lwin, 'a', 0);
	assert_string_doesnt_contain("m", response_options);
	assert_string_doesnt_contain("M", response_options);
	/* This is for files, but won't hurt to check. */
	assert_string_doesnt_contain("a", response_options);

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(rmdir(SANDBOX_PATH "/dir/sub"));
	assert_success(rmdir(SANDBOX_PATH "/dir"));
	assert_success(rmdir(SANDBOX_PATH "/sub"));
}

static void
line_prompt(const char prompt[], const char filename[], fo_prompt_cb cb,
		void *cb_arg, fo_complete_cmd_func complete)
{
	assert_fail("No prompts are expected in this fixture!");
}

static char
options_prompt_abort_and_record(const custom_prompt_t *details)
{
	const response_variant *variants = details->variants;

	int options_count = 0;
	while(variants->key != '\0')
	{
		if(options_count + 1 == sizeof(response_options))
		{
			assert_fail("Can't fit all prompt options!");
			break;
		}

		response_options[options_count++] = (variants++)->key;
	}

	response_options[options_count] = '\0';

	return '\x03';
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
