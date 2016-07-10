#include <stic.h>

#include <unistd.h> /* chdir() unlink() */

#include <stdio.h> /* fclose() fopen() fprintf() */

#include "../../src/cfg/config.h"
#include "../../src/utils/str.h"
#include "../../src/utils/utils.h"

static void line_handler(const char line[], void *arg);

static int nlines;

TEST(check_null_separation)
{
	FILE *const f = fopen(SANDBOX_PATH "/list", "w");
	fprintf(f, "%s%c", SANDBOX_PATH "/a\nb", '\0');
	fclose(f);

	assert_success(chdir(SANDBOX_PATH));

	replace_string(&cfg.shell, "/bin/sh");
	stats_update_shell_type(cfg.shell);

	nlines = 0;
	assert_success(process_cmd_output("tests", "cat list", 1, &line_handler,
				NULL));
	assert_int_equal(1, nlines);

	stats_update_shell_type("/bin/sh");
	update_string(&cfg.shell, NULL);

	assert_success(unlink("list"));
}

static void
line_handler(const char line[], void *arg)
{
	++nlines;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
