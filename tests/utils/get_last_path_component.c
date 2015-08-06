#include <stic.h>

#include "../../src/utils/path.h"

TEST(empty_path_ok)
{
	const char *const path = "";
	const char *const last = get_last_path_component(path);
	assert_string_equal(path, last);
	assert_true(path == last);
}

TEST(no_slashes_ok)
{
	const char *const path = "path_without_slashes";
	const char *const last = get_last_path_component(path);
	assert_string_equal(path, last);
	assert_true(path == last);
}

TEST(path_does_not_end_with_slash_ok)
{
	const char *const path = "/no/slashhere";
	const char *const last = get_last_path_component(path);
	assert_string_equal(path + 4, last);
	assert_true(path + 4 == last);
}

TEST(path_ends_with_slash_ok)
{
	const char *const path = "/slash/here/";
	const char *const last = get_last_path_component(path);
	assert_string_equal(path + 7, last);
	assert_true(path + 7 == last);
}

TEST(path_ends_with_multiple_slashes_ok)
{
	const char *const path = "/slash/here/////";
	const char *const last = get_last_path_component(path);
	assert_string_equal(path + 7, last);
	assert_true(path + 7 == last);
}

TEST(path_has_separators_of_multiple_slashes_ok)
{
	const char *const path = "/slashes///here";
	const char *const last = get_last_path_component(path);
	assert_string_equal(path + 11, last);
	assert_true(path + 11 == last);
}

TEST(one_element_ok)
{
	const char *const path = "/slashes";
	const char *const last = get_last_path_component(path);
	assert_string_equal(path + 1, last);
	assert_true(path + 1 == last);
}

TEST(root_ok)
{
	const char *const path = "/";
	const char *const last = get_last_path_component(path);
	assert_string_equal(path, last);
	assert_true(path == last);
}

TEST(rel_single_no_trailing_slash)
{
	assert_string_equal("c", get_last_path_component("c"));
	assert_string_equal("cc", get_last_path_component("cc"));
}

TEST(rel_single_trailing_slash)
{
	assert_string_equal("c/", get_last_path_component("c/"));
	assert_string_equal("cc/", get_last_path_component("cc/"));
}

TEST(rel_many_no_trailing_slash)
{
	assert_string_equal("c", get_last_path_component("a/b/c"));
	assert_string_equal("cc", get_last_path_component("a/b/cc"));
}

TEST(rel_many_trailing_slash)
{
	assert_string_equal("c/", get_last_path_component("a/b/c/"));
	assert_string_equal("cc/", get_last_path_component("a/b/cc/"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
