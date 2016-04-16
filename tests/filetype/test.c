#include "test.h"

#include "../../src/utils/matcher.h"
#include "../../src/filetype.h"

void
set_programs(const char pattern[], const char programs[], int for_x, int in_x)
{
	char *error;
	matcher_t *m;

	assert_non_null(m = matcher_alloc(pattern, 0, 1, "", &error));
	assert_null(error);

	ft_set_programs(m, programs, for_x, in_x);
}

void
set_viewers(const char pattern[], const char viewers[])
{
	char *error;
	matcher_t *m;

	assert_non_null(m = matcher_alloc(pattern, 0, 1, "", &error));
	assert_null(error);

	ft_set_viewers(m, viewers);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
