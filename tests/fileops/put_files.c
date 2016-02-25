#include <stic.h>

#include <sys/stat.h> /* stat */
#include <unistd.h> /* stat() */

#include <string.h> /* strcpy() */

#include "../../src/fileops.h"
#include "../../src/registers.h"

static void line_prompt(const char prompt[], const char filename[],
		fo_prompt_cb cb, fo_complete_cmd_func complete, int allow_ee);
static char options_prompt_rename(const char title[], const char message[],
		const struct response_variant *variants);
static char options_prompt_overwrite(const char title[], const char message[],
		const struct response_variant *variants);

SETUP()
{
	regs_init();

	strcpy(lwin.curr_dir, SANDBOX_PATH);
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
	assert_true(put_files_bg(&lwin, -1, 0));
}

TEST(put_files_bg_fails_on_empty_register)
{
	assert_true(put_files_bg(&lwin, 'a', 0));
}

TEST(put_files_bg_fails_on_identical_names_in_a_register)
{
	assert_success(regs_append('a', TEST_DATA_PATH "/existing-files/a"));
	assert_success(regs_append('a', TEST_DATA_PATH "/rename/a"));

	assert_true(put_files_bg(&lwin, 'a', 0));
}

TEST(overwrite_request_accounts_for_target_file_rename)
{
	struct stat st;
	FILE *f;

	f = fopen(SANDBOX_PATH "/binary-data", "w");
	fclose(f);

	f = fopen(SANDBOX_PATH "/b", "w");
	fclose(f);

	assert_success(regs_append('a', TEST_DATA_PATH "/read/binary-data"));

	init_fileops(&line_prompt, &options_prompt_rename);

	(void)put_files(&lwin, 'a', 0);

	assert_success(stat(SANDBOX_PATH "/binary-data", &st));
	assert_int_equal(0, st.st_size);

	assert_success(stat(SANDBOX_PATH "/b", &st));
	assert_int_equal(1024, st.st_size);

	(void)remove(SANDBOX_PATH "/binary-data");
	(void)remove(SANDBOX_PATH "/b");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
