#include "test.h"

#include "../../src/int/file_magic.h"
#include "../../src/utils/matchers.h"
#include "../../src/filetype.h"

void
set_programs(const char pattern[], const char programs[], int for_x, int in_x)
{
	char *error;
	matchers_t *ms;

	assert_non_null(ms = matchers_alloc(pattern, 0, 1, "", &error));
	assert_null(error);

	ft_set_programs(ms, programs, for_x, in_x);
}

void
set_viewers(const char pattern[], const char viewers[])
{
	char *error;
	matchers_t *ms;

	assert_non_null(ms = matchers_alloc(pattern, 0, 1, "", &error));
	assert_null(error);

	ft_set_viewers(ms, viewers);
}

int
has_mime_type_detection(void)
{
	return get_mimetype(TEST_DATA_PATH "/read/dos-line-endings", 0) != NULL;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
