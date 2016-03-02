#include <stic.h>

#include <stdlib.h>

#include "../../src/engine/completion.h"

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

TEST(order)
{
	char *buf;

	assert_int_equal(0, vle_compl_add_match("aa", ""));
	assert_int_equal(0, vle_compl_add_match("az", ""));
	vle_compl_finish_group();

	assert_int_equal(0, vle_compl_add_last_match("a"));

	vle_compl_set_order(1);

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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
