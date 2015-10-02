#include <stic.h>

#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/fileops.h"
#include "../../src/registers.h"

SETUP()
{
	init_registers();
}

TEARDOWN()
{
	clear_registers();
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
	assert_success(append_to_register('a', TEST_DATA_PATH "/existing-files/a"));
	assert_success(append_to_register('a', TEST_DATA_PATH "/rename/a"));

	assert_true(put_files_bg(&lwin, 'a', 0));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
