#include "seatest.h"

#include "../../src/fileops.h"

static void
test_slash_at_the_end(void)
{
	assert_string_equal("dir(1)", gen_clone_name("dir/"));
}

static void
test_no_ext(void)
{
	assert_string_equal("name(1)", gen_clone_name("name"));
}

static void
test_ext(void)
{
	assert_string_equal("name(1).ext", gen_clone_name("name.ext"));
}

static void
test_two_exts(void)
{
	assert_string_equal("name.ext1(1).ext2", gen_clone_name("name.ext1.ext2"));
}

static void
test_tar_as_2nd_ext(void)
{
	assert_string_equal("name(1).tar.ext2", gen_clone_name("name.tar.ext2"));
}

static void
test_name_inc(void)
{
	assert_string_equal("name(0)(1).tar.ext2",
			gen_clone_name("name(0).tar.ext2"));
	assert_string_equal("name(-1)(1).tar.ext2",
			gen_clone_name("name(-1).tar.ext2"));
	assert_string_equal("name(2).tar.ext2", gen_clone_name("name(1).tar.ext2"));
}

void
gen_clone_name_tests(void)
{
	test_fixture_start();

	run_test(test_slash_at_the_end);
  run_test(test_no_ext);
  run_test(test_ext);
	run_test(test_two_exts);
	run_test(test_tar_as_2nd_ext);
	run_test(test_name_inc);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
