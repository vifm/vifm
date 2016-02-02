#include <stic.h>

#include <stdio.h> /* FILE fclose() fopen() */

#include "../../src/cfg/config.h"
#include "../../src/ui/quickview.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/file_streams.h"

static void check_only_one_line_displayed(void);

TEST(no_extra_line_with_extra_padding)
{
	cfg.extra_padding = 1;
	lwin.window_rows = 2;
	check_only_one_line_displayed();
}

TEST(no_extra_line_without_extra_padding)
{
	cfg.extra_padding = 0;
	lwin.window_rows = 0;
	check_only_one_line_displayed();
}

static void
check_only_one_line_displayed(void)
{
	char line[128];
	FILE *fp;

	fp = fopen(TEST_DATA_PATH "/read/two-lines", "r");

	other_view = &lwin;

	view_stream(fp, 0);

	line[0] = '\0';
	assert_non_null(get_line(fp, line, sizeof(line)));
	assert_string_equal("2nd line\n", line);

	fclose(fp);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
