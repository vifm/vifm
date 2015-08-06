#include <stic.h>

#include "../../src/utils/filter.h"

TEST(filter_matches_differently_after_assign)
{
	filter_t src, dst;
	assert_success(filter_init(&src, 1));
	assert_success(filter_init(&dst, 1));

	assert_success(filter_set(&src, "abcd"));
	assert_success(filter_set(&dst, "xyz"));

	assert_false(filter_matches(&dst, "abcd"));
	assert_success(filter_assign(&dst, &src));
	assert_true(filter_matches(&dst, "abcd"));

	filter_dispose(&src);
	filter_dispose(&dst);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
