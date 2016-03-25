#include <stic.h>

#include <stdlib.h> /* free() */

#include "../../src/bracket_notation.h"

TEST(non_ascii_chars_are_handled_correctly)
{
	char *const spec = wstr_to_spec(L"П");
	assert_string_equal("П", spec);
	free(spec);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
