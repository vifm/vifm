#include <stic.h>

#include <stdio.h> /* remove() */
#include <stdlib.h> /* free() */

#include <test-utils.h>

#include "../../src/int/file_magic.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/matcher.h"
#include "../../src/utils/str.h"

static void check_glob(matcher_t *m);
static void check_fast_globs(matcher_t *m);
static void check_regexp(matcher_t *m);
static int has_mime_type_detection(void);
static int has_mime_type_detection_and_symlinks(void);

TEST(empty_matcher_can_be_created)
{
	char *error;
	matcher_t *m;

	assert_non_null(m = matcher_alloc("", 0, 0, "", &error));
	assert_true(matcher_is_empty(m));

	assert_false(matcher_matches(m, ""));
	assert_false(matcher_matches(m, "a"));

	matcher_free(m);
}

TEST(empty_matcher_matches_nothing_can_be_created)
{
	char *error;
	matcher_t *m = matcher_alloc("", 0, 0, "", &error);
	assert_true(matcher_is_empty(m));
	assert_string_equal(NULL, error);

	matcher_free(m);
}

TEST(glob)
{
	char *error;
	matcher_t *m;

	assert_non_null(m = matcher_alloc("{*.ext}", 0, 1, "", &error));
	assert_null(error);

	check_glob(m);

	matcher_free(m);
}

TEST(regexp)
{
	char *error;
	matcher_t *m;

	assert_non_null(m = matcher_alloc("/^x*$/", 0, 1, "", &error));
	assert_null(error);

	check_regexp(m);

	matcher_free(m);
}

TEST(defaulted_glob)
{
	char *error;
	matcher_t *m;

	assert_non_null(m = matcher_alloc("*.ext", 0, 1, "", &error));
	assert_null(error);

	check_glob(m);

	matcher_free(m);
}

TEST(defaulted_regexp)
{
	char *error;
	matcher_t *m;

	assert_non_null(m = matcher_alloc("^x*$", 0, 0, "", &error));
	assert_null(error);

	check_regexp(m);

	matcher_free(m);
}

TEST(full_path_glob)
{
	char *error;
	matcher_t *m;

	assert_non_null(m = matcher_alloc("{{/tmp/[^/].ext}}", 0, 1, "", &error));
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

	assert_non_null(m = matcher_alloc("//^/tmp/[^/]+\\.ext$//", 0, 1, "",
				&error));
	assert_null(error);

	assert_true(matcher_matches(m, "/tmp/a.ext"));
	assert_true(matcher_matches(m, "/tmp/b.ext"));
	assert_true(matcher_matches(m, "/tmp/ab.ext"));

	assert_false(matcher_matches(m, "/tmp/a,ext"));
	assert_false(matcher_matches(m, "/tmp/prog/a.ext"));
	assert_false(matcher_matches(m, "/tmp/.ext"));

	matcher_free(m);
}

TEST(matcher_negation)
{
	char *error;
	matcher_t *m;

	assert_non_null(m = matcher_alloc("!{*.ext}", 0, 1, "", &error));
	assert_null(error);
	assert_true(matcher_matches(m, "file.ext2"));
	assert_false(matcher_matches(m, "name.ext"));
	matcher_free(m);

	assert_non_null(m = matcher_alloc("!/^x*$/", 0, 1, "", &error));
	assert_null(error);
	assert_true(matcher_matches(m, "axxxxx"));
	assert_false(matcher_matches(m, "xxxxx"));
	matcher_free(m);

	assert_non_null(m = matcher_alloc("!*.ext", 0, 1, "", &error));
	assert_null(error);
	assert_true(matcher_matches(m, "!abc.ext"));
	assert_false(matcher_matches(m, "!abc.ext2"));
	matcher_free(m);

	assert_non_null(m = matcher_alloc("!x*$", 0, 0, "", &error));
	assert_null(error);
	assert_true(matcher_matches(m, "a!xx"));
	assert_false(matcher_matches(m, "xx"));
	matcher_free(m);

	assert_non_null(m = matcher_alloc("!{{/tmp/[^/].ext}}", 0, 1, "", &error));
	assert_null(error);
	assert_true(matcher_matches(m, "/tmp/a.ext1"));
	assert_false(matcher_matches(m, "/tmp/a.ext"));
	matcher_free(m);

	assert_non_null(m = matcher_alloc("!//^/tmp/[^/]+\\.ext$//", 0, 1, "",
				&error));
	assert_null(error);
	assert_true(matcher_matches(m, "/bin/ab.ext"));
	assert_false(matcher_matches(m, "/tmp/ab.ext"));
	matcher_free(m);
}

TEST(empty_regexp)
{
	char *error;
	matcher_t *m;

	assert_non_null(m = matcher_alloc("", 0, 0, ".*\\.ext", &error));
	assert_null(error);
	assert_true(matcher_matches(m, "/tmp/a.ext"));
	assert_false(matcher_matches(m, "/tmp/a.axt"));
	assert_string_equal(".*\\.ext", matcher_get_expr(m));
	assert_string_equal(".*\\.ext", matcher_get_undec(m));
	matcher_free(m);

	assert_non_null(m = matcher_alloc("//", 0, 1, ".*\\.ext", &error));
	assert_null(error);
	assert_true(matcher_matches(m, "/tmp/a.ext"));
	assert_false(matcher_matches(m, "/tmp/a.axt"));
	assert_string_equal("/.*\\.ext/", matcher_get_expr(m));
	assert_string_equal(".*\\.ext", matcher_get_undec(m));
	matcher_free(m);

	assert_non_null(m = matcher_alloc("//i", 0, 1, ".*\\.ext", &error));
	assert_null(error);
	assert_true(matcher_matches(m, "/tmp/a.Ext"));
	assert_false(matcher_matches(m, "/tmp/a.axt"));
	assert_string_equal("/.*\\.ext/i", matcher_get_expr(m));
	assert_string_equal(".*\\.ext", matcher_get_undec(m));
	matcher_free(m);

	assert_non_null(m = matcher_alloc("//Iii", 0, 1, ".*\\.ext", &error));
	assert_null(error);
	assert_true(matcher_matches(m, "/tmp/a.Ext"));
	assert_false(matcher_matches(m, "/tmp/a.axt"));
	assert_string_equal("/.*\\.ext/Iii", matcher_get_expr(m));
	assert_string_equal(".*\\.ext", matcher_get_undec(m));
	matcher_free(m);

	assert_non_null(m = matcher_alloc("////I", 0, 1, "tmp/.*\\.Ext", &error));
	assert_null(error);
	assert_true(matcher_matches(m, "/tmp/a.Ext"));
	assert_false(matcher_matches(m, "/tmp/a.axt"));
	assert_string_equal("//tmp/.*\\.Ext//I", matcher_get_expr(m));
	assert_string_equal("tmp/.*\\.Ext", matcher_get_undec(m));
	matcher_free(m);
}

TEST(wrong_regex_flag)
{
	char *error;
	matcher_t *m;

	assert_null(m = matcher_alloc("/reg/x", 0, 1, ".*\\.ext", &error));
	assert_non_null(error);
	free(error);
}

TEST(expr_includes_itself)
{
	char *error;
	matcher_t *m;

	assert_non_null(m = matcher_alloc("*.c", 0, 1, "", &error));
	assert_null(error);

	assert_true(matcher_includes(m, m));

	matcher_free(m);
}

TEST(different_exprs_match_inclusion)
{
	char *error;
	matcher_t *m1, *m2;

	assert_non_null(m1 = matcher_alloc("*.c", 0, 1, "", &error));
	assert_null(error);
	assert_non_null(m2 = matcher_alloc("/.*\\.c/", 0, 1, "", &error));
	assert_null(error);

	assert_false(matcher_includes(m1, m2));

	matcher_free(m2);
	matcher_free(m1);
}

TEST(global_match_inclusion)
{
	char *error;
	matcher_t *m1, *m2;

	assert_non_null(m1 = matcher_alloc("*.cpp,*.c", 0, 1, "", &error));
	assert_null(error);
	assert_non_null(m2 = matcher_alloc("*.c", 0, 1, "", &error));
	assert_null(error);

	assert_true(matcher_includes(m1, m2));

	matcher_free(m2);
	matcher_free(m1);
}

TEST(global_match_no_inclusion)
{
	char *error;
	matcher_t *m1, *m2;

	assert_non_null(m1 = matcher_alloc("*.cpp,*.c", 0, 1, "", &error));
	assert_null(error);
	assert_non_null(m2 = matcher_alloc("*.hpp", 0, 1, "", &error));
	assert_null(error);

	assert_false(matcher_includes(m1, m2));

	matcher_free(m2);
	matcher_free(m1);
}

TEST(regex_inclusion_case_is_taken_into_account)
{
	char *error;
	matcher_t *m1, *m2;

	assert_non_null(m1 = matcher_alloc("/a/I", 0, 1, "", &error));
	assert_null(error);
	assert_non_null(m2 = matcher_alloc("/A/I", 0, 1, "", &error));
	assert_null(error);

	assert_false(matcher_includes(m1, m2));

	matcher_free(m2);
	matcher_free(m1);
}

TEST(globs_are_cloned)
{
	char *error;
	matcher_t *m, *clone;

	assert_non_null(m = matcher_alloc("{*.ext}", 0, 1, "", &error));
	assert_null(error);
	assert_non_null(clone = matcher_clone(m));

	check_glob(m);
	matcher_free(m);

	check_glob(clone);
	matcher_free(clone);
}

TEST(globs_are_case_insensitive)
{
	char *error;
	matcher_t *m, *clone;

	assert_non_null(m = matcher_alloc("{*.ExT}", 0, 1, "", &error));
	assert_null(error);
	assert_non_null(clone = matcher_clone(m));

	check_glob(m);
	matcher_free(m);

	check_glob(clone);
	matcher_free(clone);
}

TEST(fast_globs)
{
	char *error;
	matcher_t *m;

	m = matcher_alloc("{*suffix,prefix*,mid*dle,literal}", 0, 1, "", &error);
	assert_true(matcher_is_fast(m));
	assert_non_null(m);
	assert_null(error);
	check_fast_globs(m);
	matcher_free(m);

	/* Control against equivalent non-fast-globs. */
	m = matcher_alloc("{*[s]uffix,[p]refix*,[m]id*dle,[l]iteral}", 0, 1, "",
			&error);
	assert_false(matcher_is_fast(m));
	assert_non_null(m);
	assert_null(error);
	check_fast_globs(m);
	matcher_free(m);

	m = matcher_alloc("{mid\\*dle}", 0, 1, "", &error);
	assert_true(matcher_is_fast(m));
	assert_true(matcher_matches(m, "mid*dle"));
	assert_false(matcher_matches(m, "middle"));
	matcher_free(m);
}

TEST(regexps_are_cloned)
{
	char *error;
	matcher_t *m, *clone;

	assert_non_null(m = matcher_alloc("/^x*$/", 0, 1, "", &error));
	assert_null(error);
	assert_non_null(clone = matcher_clone(m));

	check_regexp(m);
	matcher_free(m);

	check_regexp(clone);
	matcher_free(clone);
}

TEST(comma_escaping)
{
	char *error;
	matcher_t *m;

	assert_non_null(m = matcher_alloc("{a,,b,*.ext,c,,d}", 0, 1, "", &error));
	assert_null(error);

	check_glob(m);
	assert_false(matcher_matches(m, "a"));
	assert_false(matcher_matches(m, "b"));
	assert_true(matcher_matches(m, "a,b"));
	assert_true(matcher_matches(m, "c,d"));

	matcher_free(m);
}

TEST(mime_type_pattern, IF(has_mime_type_detection))
{
	char *error;
	matcher_t *m;

	assert_non_null(m = matcher_alloc("<text/plain>", 0, 1, "", &error));
	assert_null(error);
	assert_true(matcher_matches(m, TEST_DATA_PATH "/read/dos-line-endings"));
	assert_false(matcher_matches(m, TEST_DATA_PATH "/read/binary-data"));
	matcher_free(m);

	assert_non_null(m = matcher_alloc("<text/*>", 0, 1, "", &error));
	assert_null(error);
	assert_true(matcher_matches(m, TEST_DATA_PATH "/read/dos-line-endings"));
	assert_false(matcher_matches(m, TEST_DATA_PATH "/read/binary-data"));
	matcher_free(m);
}

TEST(mime_type_inclusion, IF(has_mime_type_detection))
{
	char *error;
	matcher_t *m, *m1, *m2;

	assert_non_null(m = matcher_alloc("<a/b,c/Dd>", 0, 1, "", &error));
	assert_null(error);
	assert_non_null(m1 = matcher_alloc("<c/dd>", 0, 1, "", &error));
	assert_null(error);
	assert_non_null(m2 = matcher_alloc("<c/d>", 0, 1, "", &error));
	assert_null(error);

	assert_true(matcher_includes(m, m));
	assert_true(matcher_includes(m, m1));
	assert_false(matcher_includes(m, m2));

	matcher_free(m2);
	matcher_free(m1);
	matcher_free(m);
}

TEST(mime_type_of_link_is_that_of_its_ultimate_target,
		IF(has_mime_type_detection_and_symlinks))
{
	char *error;
	matcher_t *m;

	assert_success(make_symlink(".", "link"));
	assert_success(make_symlink("link", "link2"));

	char *expr = format_str("<%s>", get_mimetype(SANDBOX_PATH, 0));

	assert_non_null(m = matcher_alloc(expr, 0, 1, "", &error));
	assert_null(error);
	assert_true(matcher_matches(m, "link"));
	assert_true(matcher_matches(m, "link2"));
	matcher_free(m);

	free(expr);

	assert_success(remove("link"));
	assert_success(remove("link2"));
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
check_fast_globs(matcher_t *m)
{
	assert_false(matcher_matches(m, "suffix"));
	assert_true(matcher_matches(m, "0suffix"));
	assert_true(matcher_matches(m, "00suffix"));
	assert_false(matcher_matches(m, "suffix0"));
	assert_false(matcher_matches(m, "0suffix0"));

	assert_true(matcher_matches(m, "prefix"));
	assert_true(matcher_matches(m, "prefix0"));
	assert_true(matcher_matches(m, "prefix00"));
	assert_false(matcher_matches(m, "0prefix"));
	assert_false(matcher_matches(m, "0prefix0"));

	assert_true(matcher_matches(m, "literal"));
	assert_false(matcher_matches(m, "0literal"));
	assert_false(matcher_matches(m, "literal0"));
	assert_false(matcher_matches(m, "0literal0"));

	assert_true(matcher_matches(m, "middle"));
	assert_true(matcher_matches(m, "mid123dle"));
	assert_false(matcher_matches(m, "0middle"));
	assert_false(matcher_matches(m, "middle0"));
	assert_false(matcher_matches(m, "0middle0"));
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

static int
has_mime_type_detection(void)
{
	return get_mimetype(TEST_DATA_PATH "/read/dos-line-endings", 0) != NULL;
}

static int
has_mime_type_detection_and_symlinks(void)
{
	return has_mime_type_detection()
	    && symlinks_available();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
