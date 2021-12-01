#include <stic.h>

#include <test-utils.h>

#include "../../src/utils/fs.h"

TEST(is_regular_file_and_symlinks, IF(not_windows))
{
	create_file(SANDBOX_PATH "/target");
	make_symlink("target", SANDBOX_PATH "/ln1");
	make_symlink("ln1", SANDBOX_PATH "/ln2");
	make_symlink("ln2", SANDBOX_PATH "/ln3");

	assert_true(is_regular_file(SANDBOX_PATH "/target"));
	assert_true(is_regular_file(SANDBOX_PATH "/ln1"));
	assert_true(is_regular_file(SANDBOX_PATH "/ln2"));
	assert_true(is_regular_file(SANDBOX_PATH "/ln3"));

	remove_file(SANDBOX_PATH "/target");
	remove_file(SANDBOX_PATH "/ln1");
	remove_file(SANDBOX_PATH "/ln2");
	remove_file(SANDBOX_PATH "/ln3");
}

TEST(is_regular_file_noderef_and_symlinks, IF(not_windows))
{
	create_file(SANDBOX_PATH "/target");
	make_symlink("target", SANDBOX_PATH "/ln1");
	make_symlink("ln1", SANDBOX_PATH "/ln2");
	make_symlink("ln2", SANDBOX_PATH "/ln3");

	assert_true(is_regular_file_noderef(SANDBOX_PATH "/target"));
	assert_false(is_regular_file_noderef(SANDBOX_PATH "/ln1"));
	assert_false(is_regular_file_noderef(SANDBOX_PATH "/ln2"));
	assert_false(is_regular_file_noderef(SANDBOX_PATH "/ln3"));

	remove_file(SANDBOX_PATH "/target");
	remove_file(SANDBOX_PATH "/ln1");
	remove_file(SANDBOX_PATH "/ln2");
	remove_file(SANDBOX_PATH "/ln3");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
