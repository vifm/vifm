#include <stic.h>

#include <locale.h> /* LC_ALL setlocale() */

#include "../../src/utils/str.h"
#include "../../src/utils/utils.h"

static int locale_works(void);

SETUP_ONCE()
{
	(void)setlocale(LC_ALL, "");
	if(!locale_works())
	{
		(void)setlocale(LC_ALL, "en_US.utf8");
	}
}

TEST(str_to_upper_ascii)
{
	char str[] = "aBcDeF";
	char buf[sizeof(str)*4];

	assert_success(str_to_upper(str, buf, sizeof(buf)));
	assert_string_equal("ABCDEF", buf);
}

TEST(str_to_upper_utf, IF(locale_works))
{
	char str[] = "АбВгДе";
	char buf[sizeof(str)*4];

	assert_success(str_to_upper(str, buf, sizeof(buf)));
	assert_string_equal("АБВГДЕ", buf);
}

TEST(str_to_upper_mixed, IF(locale_works))
{
	char str[] = "АaбbВcгdДeеf";
	char buf[sizeof(str)*4];

	assert_success(str_to_upper(str, buf, sizeof(buf)));
	assert_string_equal("АAБBВCГDДEЕF", buf);
}

TEST(str_to_lower_ascii)
{
	char str[] = "aBcDeF";
	char buf[sizeof(str)*4];

	assert_success(str_to_lower(str, buf, sizeof(buf)));
	assert_string_equal("abcdef", buf);
}

TEST(str_to_lower_utf, IF(locale_works))
{
	char str[] = "АбВгДе";
	char buf[sizeof(str)*4];

	assert_success(str_to_lower(str, buf, sizeof(buf)));
	assert_string_equal("абвгде", buf);
}

TEST(str_to_lower_mixed, IF(locale_works))
{
	char str[] = "АaбbВcгdДeеf";
	char buf[sizeof(str)*4];

	assert_success(str_to_lower(str, buf, sizeof(buf)));
	assert_string_equal("аaбbвcгdдeеf", buf);
}

TEST(str_to_lower_too_short_ascii)
{
	char str[] = "abcdef";
	char buf[sizeof(str) - 1];

	assert_failure(str_to_lower(str, buf, sizeof(buf)));
	assert_string_equal("abcde", buf);
}

TEST(str_to_upper_too_short_ascii)
{
	char str[] = "abcdef";
	char buf[sizeof(str) - 1];

	assert_failure(str_to_upper(str, buf, sizeof(buf)));
	assert_string_equal("ABCDE", buf);
}

TEST(str_to_lower_too_short_utf, IF(locale_works))
{
	char str[] = "абвг";
	char buf[sizeof(str) - 1];

	assert_failure(str_to_lower(str, buf, sizeof(buf)));
	assert_string_equal("абв", buf);
}

TEST(str_to_upper_too_short_utf, IF(locale_works))
{
	char str[] = "абвг";
	char buf[sizeof(str) - 1];

	assert_failure(str_to_upper(str, buf, sizeof(buf)));
	assert_string_equal("АБВ", buf);
}

TEST(str_to_lower_too_short_mixed, IF(locale_works))
{
	char str[] = "аaбbвcгd";
	char buf[sizeof(str) - 1];

	assert_failure(str_to_lower(str, buf, sizeof(buf)));
	assert_string_equal("аaбbвcг", buf);
}

TEST(str_to_upper_too_short_mixed, IF(locale_works))
{
	char str[] = "аaбbвcгd";
	char buf[sizeof(str) - 1];

	assert_failure(str_to_upper(str, buf, sizeof(buf)));
	assert_string_equal("АAБBВCГ", buf);
}

static int
locale_works(void)
{
	return (vifm_wcwidth(L'丝') == 2);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
