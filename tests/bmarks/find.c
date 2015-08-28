#include <stic.h>

#include <stddef.h> /* NULL */

#include "../../src/bmarks.h"

static void bmarks_cb(const char path[], const char tags[], time_t timestamp,
		void *arg);

static int nmatches;

TEST(finds_nothing_for_empty_tags)
{
	assert_success(bmarks_set("finds/nothing/for/empty/tags", "a,b,c"));

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
	assert_success(bmarks_set("finds/nothing", "a,b,c"));

	nmatches = 0;
	bmarks_find("d", &bmarks_cb, NULL);
	assert_int_equal(0, nmatches);
}

TEST(finds_single_match)
{
	assert_success(bmarks_set("finds/single/match1", "a,b,c"));
	assert_success(bmarks_set("finds/single/match2", "d,e,f"));

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
	assert_success(bmarks_set("finds/multiple/matches1", "a,b,c,x"));
	assert_success(bmarks_set("finds/multiple/matches2", "d,e,f,x"));

	nmatches = 0;
	bmarks_find("x", &bmarks_cb, NULL);
	assert_int_equal(2, nmatches);

	nmatches = 0;
	bmarks_find("x,x", &bmarks_cb, NULL);
	assert_int_equal(2, nmatches);
}

TEST(finds_intersection_of_matches)
{
	assert_success(bmarks_set("finds/intersection/of/matches1", "a,b,c,x"));
	assert_success(bmarks_set("finds/intersection/of/matches2", "d,e,f,x"));

	nmatches = 0;
	bmarks_find("a,e", &bmarks_cb, NULL);
	assert_int_equal(0, nmatches);

	nmatches = 0;
	bmarks_find("f,b,d", &bmarks_cb, NULL);
	assert_int_equal(0, nmatches);
}

static void
bmarks_cb(const char path[], const char tags[], time_t timestamp, void *arg)
{
	++nmatches;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
