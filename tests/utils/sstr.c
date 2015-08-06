#include <stic.h>

#include <stddef.h> /* size_t */

#include "../../src/utils/str.h"

TEST(nothing_appended_to_single_char_string)
{
	char str[1];
	size_t len = 0U;

	assert_failure(sstrappendch(str, &len, sizeof(str), '1'));
	assert_string_equal("", str);
	assert_int_equal(0, len);
}

TEST(char_is_appended)
{
	char str[2];
	size_t len = 0U;

	assert_success(sstrappendch(str, &len, sizeof(str), '1'));
	assert_string_equal("1", str);
	assert_int_equal(1, len);

	assert_failure(sstrappendch(str, &len, sizeof(str), '1'));
	assert_string_equal("1", str);
	assert_int_equal(1, len);
}

TEST(suffix_is_appended)
{
	char str[10];
	size_t len = 0U;

	assert_success(sstrappend(str, &len, sizeof(str), "12345"));
	assert_string_equal("12345", str);
	assert_int_equal(5, len);

	assert_failure(sstrappend(str, &len, sizeof(str), "67890"));
	assert_string_equal("123456789", str);
	assert_int_equal(sizeof(str) - 1, len);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
