#include <stic.h>

#include <stddef.h> /* NULL */

#include "../../src/bmarks.h"

static void bmarks_cb(const char path[], const char tags[], void *arg);

static int nmatches;

TEST(finds_nothing_for_empty_tags)
{
	bmarks_set("finds/nothing/for/empty/tags", "a,b,c");

	nmatches = 0;
	bmarks_find("", &bmarks_cb, NULL);
	assert_int_equal(0, nmatches);

	nmatches = 0;
	bmarks_find(",", &bmarks_cb, NULL);
	assert_int_equal(0, nmatches);

	nmatches = 0;
	bmarks_find(",,", &bmarks_cb, NULL);
	assert_int_equal(0, nmatches);
}

TEST(finds_nothing)
{
	bmarks_set("finds/nothing", "a,b,c");

	nmatches = 0;
	bmarks_find("d", &bmarks_cb, NULL);
	assert_int_equal(0, nmatches);
}

TEST(finds_single_match)
{
	bmarks_set("finds/single/match1", "a,b,c");
	bmarks_set("finds/single/match2", "d,e,f");

	nmatches = 0;
	bmarks_find("d", &bmarks_cb, NULL);
	assert_int_equal(1, nmatches);

	nmatches = 0;
	bmarks_find("d,e", &bmarks_cb, NULL);
	assert_int_equal(1, nmatches);

	nmatches = 0;
	bmarks_find("a,b", &bmarks_cb, NULL);
	assert_int_equal(1, nmatches);
}

TEST(finds_multiple_matches)
{
	bmarks_set("finds/multiple/matches1", "a,b,c,x");
	bmarks_set("finds/multiple/matches2", "d,e,f,x");

	nmatches = 0;
	bmarks_find("x,x", &bmarks_cb, NULL);
	assert_int_equal(2, nmatches);

	nmatches = 0;
	bmarks_find("a,e", &bmarks_cb, NULL);
	assert_int_equal(2, nmatches);

	nmatches = 0;
	bmarks_find("f,b,d", &bmarks_cb, NULL);
	assert_int_equal(2, nmatches);
}

static void
bmarks_cb(const char path[], const char tags[], void *arg)
{
	++nmatches;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
