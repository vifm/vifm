#include <stic.h>

#include <stdlib.h> /* free() */

#include "../../src/utils/str.h"

TEST(no_formatting_ok)
{
#define STR "No formatting"
	char *str = format_str(STR);
	assert_string_equal(STR, str);
	free(str);
#undef STR
}

TEST(formatting_ok)
{
	char *str = format_str("String %swith%s formatting", "'", "'");
	assert_string_equal("String 'with' formatting", str);
	free(str);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
