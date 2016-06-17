#include <stic.h>

#include <stddef.h> /* NULL size_t */
#include <stdlib.h> /* free() */

#include "../../src/utils/str.h"

TEST(can_prepend_to_an_empty_string)
{
	char *str = NULL;
	size_t len = 0U;

	assert_success(strprepend(&str, &len, "a"));
	assert_string_equal("a", str);
	free(str);
}

TEST(can_prepend_multiple_times_in_a_row)
{
	char *str = NULL;
	size_t len = 0U;

	assert_success(strprepend(&str, &len, "a"));
	assert_string_equal("a", str);
	assert_success(strprepend(&str, &len, "bb"));
	assert_string_equal("bba", str);
	assert_success(strprepend(&str, &len, "c"));
	assert_string_equal("cbba", str);
	free(str);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
