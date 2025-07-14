#include "test.h"

#include "../../src/int/file_magic.h"
#include "../../src/filetype.h"

void
set_programs(const char pattern[], const char programs[], int for_x, int in_x)
{
	char *error;
	matchers_group_t mg;
	assert_success(ft_mg_from_string(pattern, &mg, &error));
	assert_string_equal(NULL, error);

	ft_set_programs(mg, programs, for_x, in_x);
}

void
set_viewers(const char pattern[], const char viewers[])
{
	char *error;
	matchers_group_t mg;
	assert_success(ft_mg_from_string(pattern, &mg, &error));
	assert_string_equal(NULL, error);

	ft_set_viewers(mg, viewers);
}

int
has_mime_type_detection(void)
{
	return get_mimetype(TEST_DATA_PATH "/read/dos-line-endings", 0) != NULL;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
