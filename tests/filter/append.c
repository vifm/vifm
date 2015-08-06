#include <stic.h>

#include <string.h>
#include <stdlib.h>

#include "../../src/utils/filter.h"

TEST(append_produces_desired_effect)
{
	filter_t filter;
	assert_int_equal(0, filter_init(&filter, 1));

	assert_int_equal(0, filter_set(&filter, "abcd"));
	assert_true(filter_matches(&filter, "abcd"));
	assert_false(filter_matches(&filter, "efgh"));

	assert_int_equal(0, filter_append(&filter, "efgh"));
	assert_true(filter_matches(&filter, "abcd"));
	assert_true(filter_matches(&filter, "efgh"));

	filter_dispose(&filter);
}

TEST(append_escapes)
{
	filter_t filter;
	assert_int_equal(0, filter_init(&filter, 1));

	assert_int_equal(0, filter_append(&filter, "ef|gh"));
	assert_true(filter_matches(&filter, "ef|gh"));

	filter_dispose(&filter);
}

TEST(empty_value_not_appended)
{
	char *initial_value;

	filter_t filter;
	assert_int_equal(0, filter_init(&filter, 1));

	initial_value = strdup(filter.raw);

	assert_int_equal(1, filter_append(&filter, ""));

	assert_string_equal(initial_value, filter.raw);

	free(initial_value);
	filter_dispose(&filter);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
