#include "seatest.h"

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() */

#include "../../src/utils/str.h"

static void
test_empty_string_null_returned(void)
{
	char input[] = "";
	char *part = input, *state = NULL;

	assert_string_equal(NULL, part = split_and_get(part, ',', &state));
}

static void
test_all_empty_items_are_skipped(void)
{
	char input[] = ",,,";
	char *part = input, *state = NULL;

	assert_string_equal(NULL, part = split_and_get(part, ',', &state));
}

static void
test_empty_items_are_skipped(void)
{
	char input[] = ",a,,b,";
	char *part = input, *state = NULL;

	assert_string_equal("a", part = split_and_get(part, ',', &state));
	assert_string_equal("b", part = split_and_get(part, ',', &state));
	assert_string_equal(NULL, part = split_and_get(part, ',', &state));
}

static void
test_single_item_returned(void)
{
	char input[] = "something";
	char *part = input, *state = NULL;

	assert_string_equal("something", part = split_and_get(part, ',', &state));
	assert_string_equal(NULL, part = split_and_get(part, ',', &state));
}

static void
test_many_items(void)
{
	char input[] = "x,yy,zzz,abc";
	char *part = input, *state = NULL;

	assert_string_equal("x", part = split_and_get(part, ',', &state));
	assert_string_equal("yy", part = split_and_get(part, ',', &state));
	assert_string_equal("zzz", part = split_and_get(part, ',', &state));
	assert_string_equal("abc", part = split_and_get(part, ',', &state));
	assert_string_equal(NULL, part = split_and_get(part, ',', &state));
}

static void
test_usage_idiom(void)
{
	char input[] = "a: bb,cc: d:d";
	const char *element[] = { "a", " bb,cc", " d", "d", };
	char *part = input, *state = NULL;
	int i;

	i = 0;
	while((part = split_and_get(part, ':', &state)) != NULL)
	{
		assert_string_equal(element[i++], part);
	}
}

static void
test_separators_are_restored(void)
{
	char input[] = "/x/yy/zzz/abc";
	char *part = input, *state = NULL;

	assert_string_equal("/x", (part = split_and_get(part, '/', &state), input));
	assert_string_equal("/x/yy", (part = split_and_get(part, '/', &state), input));
	assert_string_equal("/x/yy/zzz", (part = split_and_get(part, '/', &state), input));
	assert_string_equal("/x/yy/zzz/abc",
			(part = split_and_get(part, '/', &state), input));
	assert_string_equal("/x/yy/zzz/abc",
			(part = split_and_get(part, '/', &state), input));
}

void
split_and_get_tests(void)
{
	test_fixture_start();

	run_test(test_empty_string_null_returned);
	run_test(test_all_empty_items_are_skipped);
	run_test(test_empty_items_are_skipped);
	run_test(test_single_item_returned);
	run_test(test_many_items);
	run_test(test_usage_idiom);
	run_test(test_separators_are_restored);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
