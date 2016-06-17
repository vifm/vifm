#include <stic.h>

#include <stddef.h> /* NULL */

#include "../../src/utils/str.h"

TEST(empty_string_null_returned)
{
	char input[] = "";
	char *part = input, *state = NULL;

	assert_string_equal(NULL, part = split_and_get_dc(part, &state));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
