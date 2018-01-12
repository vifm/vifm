#include <stic.h>

#include "../../src/fops_misc.h"

TEST(slash_at_the_end)
{
	assert_string_equal("dir(1)", gen_clone_name(".", "dir/"));
}

TEST(no_ext)
{
	assert_string_equal("name(1)", gen_clone_name(".", "name"));
}

TEST(ext)
{
	assert_string_equal("name(1).ext", gen_clone_name(".", "name.ext"));
}

TEST(two_exts)
{
	assert_string_equal("name.ext1(1).ext2",
			gen_clone_name(".", "name.ext1.ext2"));
}

TEST(tar_as_2nd_ext)
{
	assert_string_equal("name(1).tar.ext2", gen_clone_name(".", "name.tar.ext2"));
}

TEST(name_inc)
{
	assert_string_equal("name(0)(1).tar.ext2",
			gen_clone_name(".", "name(0).tar.ext2"));
	assert_string_equal("name(-1)(1).tar.ext2",
			gen_clone_name(".", "name(-1).tar.ext2"));
	assert_string_equal("name(2).tar.ext2",
			gen_clone_name(".", "name(1).tar.ext2"));
}

TEST(dot_files)
{
	assert_string_equal(".name(1)", gen_clone_name(".", ".name"));
	assert_string_equal(".name(1).ext", gen_clone_name(".", ".name.ext"));
	assert_string_equal(".name(2)", gen_clone_name(".", ".name(1)"));

	assert_string_equal(".config(1).d", gen_clone_name(".", ".config.d"));
	assert_string_equal(".config(2).d", gen_clone_name(".", ".config(1).d"));
	assert_string_equal(".config(1).d(1)", gen_clone_name(".", ".config.d(1)"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
