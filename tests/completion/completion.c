#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h> /* strcmp() */

#include "../../src/engine/completion.h"

#include <test-utils.h>

static int reverse_sorter(const char a[], const char b[]);

SETUP_ONCE()
{
	try_enable_utf8_locale();
}

TEST(general)
{
	char *buf;

	assert_int_equal(0, vle_compl_add_match("abc", ""));
	assert_int_equal(0, vle_compl_add_match("acd", ""));
	vle_compl_finish_group();

	assert_int_equal(0, vle_compl_add_last_match("a"));

	buf = vle_compl_next();
	assert_string_equal("abc", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("acd", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("a", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("abc", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("acd", buf);
	free(buf);
}

TEST(sorting)
{
	char *buf;

	assert_int_equal(0, vle_compl_add_match("acd", ""));
	assert_int_equal(0, vle_compl_add_match("abc", ""));
	vle_compl_finish_group();

	assert_int_equal(0, vle_compl_add_last_match("a"));

	buf = vle_compl_next();
	assert_string_equal("abc", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("acd", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("a", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("abc", buf);
	free(buf);
}

TEST(custom_sorter)
{
	char *buf;

	vle_compl_set_sorter(&reverse_sorter);

	assert_int_equal(0, vle_compl_add_match("a", ""));
	assert_int_equal(0, vle_compl_add_match("b", ""));
	assert_int_equal(0, vle_compl_add_match("c", ""));
	vle_compl_finish_group();

	assert_int_equal(0, vle_compl_add_last_match("x"));

	buf = vle_compl_next();
	assert_string_equal("c", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("b", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("a", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("x", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("c", buf);
	free(buf);
}

TEST(one_element_completion)
{
	char *buf;

	vle_compl_finish_group();

	assert_int_equal(0, vle_compl_add_match("a", ""));

	buf = vle_compl_next();
	assert_string_equal("a", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("a", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("a", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("a", buf);
	free(buf);
}

TEST(one_unambiguous_completion)
{
	char *buf;

	assert_int_equal(0, vle_compl_add_match("abcd", ""));
	vle_compl_finish_group();

	assert_int_equal(0, vle_compl_add_last_match("a"));

	buf = vle_compl_next();
	assert_string_equal("abcd", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("abcd", buf);
	free(buf);
}

TEST(rewind_one_unambiguous_completion)
{
	char *buf;

	assert_int_equal(0, vle_compl_add_match("abcd", ""));
	vle_compl_finish_group();

	assert_int_equal(0, vle_compl_add_last_match("a"));

	buf = vle_compl_next();
	assert_string_equal("abcd", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("abcd", buf);
	free(buf);

	vle_compl_rewind();

	buf = vle_compl_next();
	assert_string_equal("a", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("abcd", buf);
	free(buf);
}

TEST(rewind_completion)
{
	char *buf;

	assert_int_equal(0, vle_compl_add_match("acd", ""));
	assert_int_equal(0, vle_compl_add_match("abc", ""));
	vle_compl_finish_group();

	assert_int_equal(0, vle_compl_add_last_match("a"));

	buf = vle_compl_next();
	assert_string_equal("abc", buf);
	free(buf);

	vle_compl_rewind();
	free(vle_compl_next());

	buf = vle_compl_next();
	assert_string_equal("abc", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("acd", buf);
	free(buf);

	vle_compl_rewind();
	free(vle_compl_next());

	buf = vle_compl_next();
	assert_string_equal("abc", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("acd", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("a", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("abc", buf);
	free(buf);
}

TEST(direction)
{
	char *buf;

	assert_int_equal(0, vle_compl_add_match("aa", ""));
	assert_int_equal(0, vle_compl_add_match("az", ""));
	vle_compl_finish_group();

	assert_int_equal(0, vle_compl_add_last_match("a"));

	vle_compl_set_reversed(1);

	buf = vle_compl_next();
	assert_string_equal("az", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("aa", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("a", buf);
	free(buf);
}

TEST(umbiguous_begin)
{
	char *buf;

	assert_int_equal(0, vle_compl_add_match("sort", ""));
	assert_int_equal(0, vle_compl_add_match("sortorder", ""));
	assert_int_equal(0, vle_compl_add_match("sortnumbers", ""));
	vle_compl_finish_group();

	assert_int_equal(0, vle_compl_add_last_match("sort"));

	buf = vle_compl_next();
	assert_string_equal("sort", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("sortnumbers", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("sortorder", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("sort", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("sort", buf);
	free(buf);
}

TEST(two_matches)
{
	char *buf;

	assert_int_equal(0, vle_compl_add_match("mountpoint", ""));
	vle_compl_finish_group();
	assert_int_equal(0, vle_compl_add_match("mount", ""));
	vle_compl_finish_group();

	assert_int_equal(0, vle_compl_add_last_match("mount"));

	buf = vle_compl_next();
	assert_string_equal("mountpoint", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("mount", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("mount", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("mountpoint", buf);
	free(buf);
}

TEST(removes_duplicates)
{
	char *buf;

	assert_int_equal(0, vle_compl_add_match("mou", ""));
	assert_int_equal(0, vle_compl_add_match("mount", ""));
	assert_int_equal(0, vle_compl_add_match("mount", ""));
	vle_compl_finish_group();

	assert_int_equal(0, vle_compl_add_last_match("m"));

	buf = vle_compl_next();
	assert_string_equal("mou", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("mount", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("m", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("mou", buf);
	free(buf);
}

TEST(unicode_sorting, IF(utf8_locale))
{
	/* Hex-coded names are öäüßÖÄÜ written with and without compount
	 * characters. */
	const char *compounds =
		"\x6f\xcc\x88\x61\xcc\x88\x75\xcc\x88\xc3\x9f\x4f\xcc\x88\x41\xcc\x88\x55"
		"\xcc\x88";
	const char *non_compounds =
		"\xc3\xb6\xc3\xa4\xc3\xbc\xc3\x9f\xc3\x96\xc3\x84\xc3\x9c";

	assert_int_equal(0, vle_compl_add_match("a", ""));
	assert_int_equal(0, vle_compl_add_match("A", ""));
	assert_int_equal(0, vle_compl_add_match(compounds, ""));
	assert_int_equal(0, vle_compl_add_match(non_compounds, ""));
	assert_int_equal(0, vle_compl_add_match("ß", ""));
	assert_int_equal(0, vle_compl_add_match("Ö", ""));
	assert_int_equal(0, vle_compl_add_match("ö", ""));
	assert_int_equal(0, vle_compl_add_match("p", ""));
	vle_compl_finish_group();

	char *buf;

	buf = vle_compl_next();
	assert_string_equal("A", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("Ö", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("a", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("ö", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal(compounds, buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal(non_compounds, buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("p", buf);
	free(buf);
}

static int
reverse_sorter(const char a[], const char b[])
{
	int r = strcmp(a, b);
	return (r < 0 ? 1 : (r > 0 ? -1 : 0));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
