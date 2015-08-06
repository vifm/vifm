#include <stic.h>

#include "../../src/utils/matcher.h"

static void check_glob(matcher_t *m);
static void check_regexp(matcher_t *m);

TEST(glob)
{
	char *error;
	matcher_t *m;

	assert_non_null(m = matcher_alloc("{*.ext}", 0, 1, &error));
	assert_null(error);

	check_glob(m);

	matcher_free(m);
}

TEST(regexp)
{
	char *error;
	matcher_t *m;

	assert_non_null(m = matcher_alloc("/^x*$/", 0, 1, &error));
	assert_null(error);

	check_regexp(m);

	matcher_free(m);
}

TEST(defaulted_glob)
{
	char *error;
	matcher_t *m;

	assert_non_null(m = matcher_alloc("*.ext", 0, 1, &error));
	assert_null(error);

	check_glob(m);

	matcher_free(m);
}

TEST(defaulted_regexp)
{
	char *error;
	matcher_t *m;

	assert_non_null(m = matcher_alloc("^x*$", 0, 0, &error));
	assert_null(error);

	check_regexp(m);

	matcher_free(m);
}

TEST(full_path_glob)
{
	char *error;
	matcher_t *m;

	assert_non_null(m = matcher_alloc("{{/tmp/[^/].ext}}", 0, 1, &error));
	assert_null(error);

	assert_true(matcher_matches(m, "/tmp/a.ext"));
	assert_true(matcher_matches(m, "/tmp/b.ext"));

	assert_false(matcher_matches(m, "/tmp/a,ext"));
	assert_false(matcher_matches(m, "/tmp/prog/a.ext"));
	assert_false(matcher_matches(m, "/tmp/.ext"));
	assert_false(matcher_matches(m, "/tmp/ab.ext"));

	matcher_free(m);
}

TEST(full_path_regexp)
{
	char *error;
	matcher_t *m;

	assert_non_null(m = matcher_alloc("//^/tmp/[^/]+\\.ext$//", 0, 1, &error));
	assert_null(error);

	assert_true(matcher_matches(m, "/tmp/a.ext"));
	assert_true(matcher_matches(m, "/tmp/b.ext"));
	assert_true(matcher_matches(m, "/tmp/ab.ext"));

	assert_false(matcher_matches(m, "/tmp/a,ext"));
	assert_false(matcher_matches(m, "/tmp/prog/a.ext"));
	assert_false(matcher_matches(m, "/tmp/.ext"));

	matcher_free(m);
}

static void
check_glob(matcher_t *m)
{
	assert_true(matcher_matches(m, "{.ext"));
	assert_true(matcher_matches(m, "}.ext"));
	assert_true(matcher_matches(m, "name.ext"));

	assert_false(matcher_matches(m, "{,ext"));
}

static void
check_regexp(matcher_t *m)
{
	assert_true(matcher_matches(m, "x"));
	assert_true(matcher_matches(m, "xx"));
	assert_true(matcher_matches(m, "xxx"));

	assert_false(matcher_matches(m, "y"));
	assert_false(matcher_matches(m, "xy"));
	assert_false(matcher_matches(m, "yx"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
