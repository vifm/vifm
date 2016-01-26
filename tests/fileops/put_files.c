#include <stic.h>

#include "../../src/fileops.h"
#include "../../src/registers.h"

SETUP()
{
	regs_init();
}

TEARDOWN()
{
	regs_reset();
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
