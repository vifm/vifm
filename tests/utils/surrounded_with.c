#include <stic.h>

#include "../../src/utils/str.h"

TEST(empty_string_false)
{
	assert_false(surrounded_with("", '{', '}'));
	assert_false(surrounded_with("", '\0', '\0'));
}

TEST(singe_character_string_false)
{
	assert_false(surrounded_with("{", '{', '}'));
	assert_false(surrounded_with("}", '{', '}'));

	assert_false(surrounded_with("/", '/', '/'));
}

TEST(empty_substring_false)
{
	assert_false(surrounded_with("{}", '{', '}'));
}

TEST(nonempty_substring_true)
{
	assert_true(surrounded_with("{a}", '{', '}'));
	assert_true(surrounded_with("{ab}", '{', '}'));
	assert_true(surrounded_with("{abc}", '{', '}'));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
