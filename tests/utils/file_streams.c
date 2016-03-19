#include <stic.h>

#include <stdio.h> /* FILE fclose() fopen() */

#include "../../src/utils/file_streams.h"

TEST(bom_is_skipped)
{
	char line[256];
	FILE *const f = fopen(TEST_DATA_PATH "/read/utf8-bom", "rb");

	skip_bom(f);

	assert_non_null(get_line(f, line, sizeof(line)));
	assert_string_equal("1\n", line);

	fclose(f);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
