#include <stic.h>

#include <locale.h> /* LC_ALL setlocale() */
#include <stdlib.h> /* free() */
#include <string.h> /* memset() strcpy() */

#include "../../src/utils/str.h"
#include "../../src/utils/utils.h"

typedef char * (*func)(const char str[], size_t max_width, const char ell[]);

static void test_ellipsis(const char src[], const char dst[], size_t width,
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
	test_ellipsis("abc", "", 0U, &left_ellipsis);
	test_ellipsis("abc", ".", 1U, &left_ellipsis);
	test_ellipsis("abc", "..", 2U, &left_ellipsis);
	test_ellipsis("abc", "abc", 3U, &left_ellipsis);

	test_ellipsis("abcde", ".", 1U, &left_ellipsis);
	test_ellipsis("abcde", "..", 2U, &left_ellipsis);
	test_ellipsis("abcde", "...", 3U, &left_ellipsis);
	test_ellipsis("abcde", "...e", 4U, &left_ellipsis);
}

TEST(left_align_ellipsis_wide, IF(locale_works))
{
	test_ellipsis("师", ".", 1U, &left_ellipsis);
	test_ellipsis("师", "师", 2U, &left_ellipsis);

	test_ellipsis("师从刀", ".", 1U, &left_ellipsis);
	test_ellipsis("师从刀", "..", 2U, &left_ellipsis);
	test_ellipsis("师从刀", "...", 3U, &left_ellipsis);
	test_ellipsis("师从刀", "...", 4U, &left_ellipsis);
	test_ellipsis("师从刀", "...刀", 5U, &left_ellipsis);
	test_ellipsis("师从刀", "师从刀", 6U, &left_ellipsis);
}

TEST(right_align_ellipsis)
{
	test_ellipsis("abc", "", 0U, &right_ellipsis);
	test_ellipsis("abc", ".", 1U, &right_ellipsis);
	test_ellipsis("abc", "..", 2U, &right_ellipsis);
	test_ellipsis("abc", "abc", 3U, &right_ellipsis);

	test_ellipsis("abcde", ".", 1U, &right_ellipsis);
	test_ellipsis("abcde", "..", 2U, &right_ellipsis);
	test_ellipsis("abcde", "...", 3U, &right_ellipsis);
	test_ellipsis("abcde", "a...", 4U, &right_ellipsis);
}

TEST(right_align_ellipsis_wide, IF(locale_works))
{
	test_ellipsis("师", ".", 1U, &right_ellipsis);
	test_ellipsis("师", "师", 2U, &right_ellipsis);

	test_ellipsis("师从刀", ".", 1U, &right_ellipsis);
	test_ellipsis("师从刀", "..", 2U, &right_ellipsis);
	test_ellipsis("师从刀", "...", 3U, &right_ellipsis);
	test_ellipsis("师从刀", "...", 4U, &right_ellipsis);
	test_ellipsis("师从刀", "师...", 5U, &right_ellipsis);
	test_ellipsis("师从刀", "师从刀", 6U, &right_ellipsis);
}

static void
test_ellipsis(const char src[], const char dst[], size_t width, func f)
{
	char *const ellipsis = f(src, width, "...");
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
