#include <stic.h>

#include <locale.h> /* LC_ALL setlocale() */
#include <stdlib.h> /* free() */
#include <string.h> /* memset() strcpy() */

#include "../../src/utils/str.h"
#include "../../src/utils/utils.h"

typedef char * (*func)(const char str[], size_t max_width, const char ell[]);

static void test_ascii(const char src[], const char dst[], size_t width,
		func f);
static void test_unicode(const char src[], const char dst[], size_t width,
		func f);
static int locale_works(void);

SETUP_ONCE()
{
	(void)setlocale(LC_ALL, "");
	if(!locale_works())
	{
		(void)setlocale(LC_ALL, "en_US.utf8");
	}
}

TEST(left_align_ellipsis)
{
	test_ascii("abc", "", 0U, &left_ellipsis);
	test_ascii("abc", ".", 1U, &left_ellipsis);
	test_ascii("abc", "..", 2U, &left_ellipsis);
	test_ascii("abc", "abc", 3U, &left_ellipsis);

	test_ascii("abcde", ".", 1U, &left_ellipsis);
	test_ascii("abcde", "..", 2U, &left_ellipsis);
	test_ascii("abcde", "...", 3U, &left_ellipsis);
	test_ascii("abcde", "...e", 4U, &left_ellipsis);
}

TEST(left_align_ellipsis_wide, IF(locale_works))
{
	test_ascii("师", ".", 1U, &left_ellipsis);
	test_ascii("师", "师", 2U, &left_ellipsis);

	test_ascii("师从刀", ".", 1U, &left_ellipsis);
	test_ascii("师从刀", "..", 2U, &left_ellipsis);
	test_ascii("师从刀", "...", 3U, &left_ellipsis);
	test_ascii("师从刀", "...", 4U, &left_ellipsis);
	test_ascii("师从刀", "...刀", 5U, &left_ellipsis);
	test_ascii("师从刀", "师从刀", 6U, &left_ellipsis);
}

TEST(right_align_ellipsis)
{
	test_ascii("abc", "", 0U, &right_ellipsis);
	test_ascii("abc", ".", 1U, &right_ellipsis);
	test_ascii("abc", "..", 2U, &right_ellipsis);
	test_ascii("abc", "abc", 3U, &right_ellipsis);

	test_ascii("abcde", ".", 1U, &right_ellipsis);
	test_ascii("abcde", "..", 2U, &right_ellipsis);
	test_ascii("abcde", "...", 3U, &right_ellipsis);
	test_ascii("abcde", "a...", 4U, &right_ellipsis);
}

TEST(right_align_ellipsis_wide, IF(locale_works))
{
	test_ascii("师", ".", 1U, &right_ellipsis);
	test_ascii("师", "师", 2U, &right_ellipsis);

	test_ascii("师从刀", ".", 1U, &right_ellipsis);
	test_ascii("师从刀", "..", 2U, &right_ellipsis);
	test_ascii("师从刀", "...", 3U, &right_ellipsis);
	test_ascii("师从刀", "...", 4U, &right_ellipsis);
	test_ascii("师从刀", "师...", 5U, &right_ellipsis);
	test_ascii("师从刀", "师从刀", 6U, &right_ellipsis);
}

TEST(left_align_unicode_ellipsis, IF(locale_works))
{
	test_unicode("abc", "", 0U, &left_ellipsis);
	test_unicode("abc", "…", 1U, &left_ellipsis);
	test_unicode("abc", "…c", 2U, &left_ellipsis);
	test_unicode("abc", "abc", 3U, &left_ellipsis);

	test_unicode("abcde", "…", 1U, &left_ellipsis);
	test_unicode("abcde", "…e", 2U, &left_ellipsis);
	test_unicode("abcde", "…de", 3U, &left_ellipsis);
	test_unicode("abcde", "…cde", 4U, &left_ellipsis);
}

TEST(left_align_unicode_ellipsis_wide, IF(locale_works))
{
	test_unicode("师", "…", 1U, &left_ellipsis);
	test_unicode("师", "师", 2U, &left_ellipsis);

	test_unicode("师从刀", "…", 1U, &left_ellipsis);
	test_unicode("师从刀", "…", 2U, &left_ellipsis);
	test_unicode("师从刀", "…刀", 3U, &left_ellipsis);
	test_unicode("师从刀", "…刀", 4U, &left_ellipsis);
	test_unicode("师从刀", "…从刀", 5U, &left_ellipsis);
	test_unicode("师从刀", "师从刀", 6U, &left_ellipsis);
}

TEST(right_align_unicode_ellipsis)
{
	test_unicode("abc", "", 0U, &right_ellipsis);
	test_unicode("abc", "…", 1U, &right_ellipsis);
	test_unicode("abc", "a…", 2U, &right_ellipsis);
	test_unicode("abc", "abc", 3U, &right_ellipsis);

	test_unicode("abcde", "…", 1U, &right_ellipsis);
	test_unicode("abcde", "a…", 2U, &right_ellipsis);
	test_unicode("abcde", "ab…", 3U, &right_ellipsis);
	test_unicode("abcde", "abc…", 4U, &right_ellipsis);
}

TEST(right_align_unicode_ellipsis_wide, IF(locale_works))
{
	test_unicode("师", "…", 1U, &right_ellipsis);
	test_unicode("师", "师", 2U, &right_ellipsis);

	test_unicode("师从刀", "…", 1U, &right_ellipsis);
	test_unicode("师从刀", "…", 2U, &right_ellipsis);
	test_unicode("师从刀", "师…", 3U, &right_ellipsis);
	test_unicode("师从刀", "师…", 4U, &right_ellipsis);
	test_unicode("师从刀", "师从…", 5U, &right_ellipsis);
	test_unicode("师从刀", "师从刀", 6U, &right_ellipsis);
}

static void
test_ascii(const char src[], const char dst[], size_t width, func f)
{
	char *const ellipsis = f(src, width, "...");
	assert_string_equal(dst, ellipsis);
	free(ellipsis);
}

static void
test_unicode(const char src[], const char dst[], size_t width, func f)
{
	char *const ellipsis = f(src, width, "…");
	assert_string_equal(dst, ellipsis);
	free(ellipsis);
}

static int
locale_works(void)
{
	return (vifm_wcwidth(L'丝') == 2);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
