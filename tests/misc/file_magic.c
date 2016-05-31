#include <stic.h>

#include <unistd.h> /* unlink() */

#include <stdio.h> /* fopen() fclose() */

#include "../../src/int/file_magic.h"

#include "utils.h"

static void check_empty_file(const char fname[]);
static int has_mime_type_detection(void);

TEST(escaping_for_determining_mime_type, IF(has_mime_type_detection))
{
	check_empty_file(SANDBOX_PATH "/start'end");
	check_empty_file(SANDBOX_PATH "/start\"end");
	check_empty_file(SANDBOX_PATH "/start`end");
}

static void
check_empty_file(const char fname[])
{
	FILE *const f = fopen(fname, "w");
	if(f != NULL)
	{
		fclose(f);
		assert_non_null(get_mimetype(fname));
		assert_success(unlink(fname));
	}
}

static int
has_mime_type_detection(void)
{
	return get_mimetype(TEST_DATA_PATH "/read/dos-line-endings") != NULL;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
