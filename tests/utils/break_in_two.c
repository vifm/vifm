#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../../src/utils/str.h"

TEST(things_are_aligned_correctly_for_two_char_separator)
{
	char *line;

	line = break_in_two(strdup("a%=b"), 1, "%=");
	assert_string_equal("b", line);
	free(line);

	line = break_in_two(strdup("a%=b"), 2, "%=");
	assert_string_equal("ab", line);
	free(line);

	line = break_in_two(strdup("a%=b"), 3, "%=");
	assert_string_equal("a b", line);
	free(line);
}

TEST(things_are_aligned_correctly_for_single_char_separator)
{
	char *line;

	line = break_in_two(strdup("a=b"), 1, "=");
	assert_string_equal("b", line);
	free(line);

	line = break_in_two(strdup("a=b"), 2, "=");
	assert_string_equal("ab", line);
	free(line);

	line = break_in_two(strdup("a=b"), 3, "=");
	assert_string_equal("a b", line);
	free(line);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
