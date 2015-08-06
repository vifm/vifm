#include <stic.h>

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() */

#include "../../src/utils/str.h"

TEST(empty_string_null_returned)
{
	char input[] = "";
	char *part = input, *state = NULL;

	assert_string_equal(NULL, part = split_and_get(part, ',', &state));
}

TEST(all_empty_items_are_skipped)
{
	char input[] = ",,,";
	char *part = input, *state = NULL;

	assert_string_equal(NULL, part = split_and_get(part, ',', &state));
}

TEST(empty_items_are_skipped)
{
	char input[] = ",a,,b,";
	char *part = input, *state = NULL;

	assert_string_equal("a", part = split_and_get(part, ',', &state));
	assert_string_equal("b", part = split_and_get(part, ',', &state));
	assert_string_equal(NULL, part = split_and_get(part, ',', &state));
}

TEST(single_item_returned)
{
	char input[] = "something";
	char *part = input, *state = NULL;

	assert_string_equal("something", part = split_and_get(part, ',', &state));
	assert_string_equal(NULL, part = split_and_get(part, ',', &state));
}

TEST(many_items)
{
	char input[] = "x,yy,zzz,abc";
	char *part = input, *state = NULL;

	assert_string_equal("x", part = split_and_get(part, ',', &state));
	assert_string_equal("yy", part = split_and_get(part, ',', &state));
	assert_string_equal("zzz", part = split_and_get(part, ',', &state));
	assert_string_equal("abc", part = split_and_get(part, ',', &state));
	assert_string_equal(NULL, part = split_and_get(part, ',', &state));
}

TEST(usage_idiom)
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

TEST(separators_are_restored)
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
