#include <stic.h>

#include <locale.h> /* LC_ALL setlocale() */

#include "../../src/utils/str.h"

SETUP_ONCE()
{
	(void)setlocale(LC_ALL, "");
}

TEST(str_to_upper_ascii)
{
	char str[] = "aBcDeF";
	char buf[sizeof(str)*4];

	assert_success(str_to_upper(str, buf, sizeof(buf)));
	assert_string_equal("ABCDEF", buf);
}

TEST(str_to_upper_utf)
{
	char str[] = "АбВгДе";
	char buf[sizeof(str)*4];

	assert_success(str_to_upper(str, buf, sizeof(buf)));
	assert_string_equal("АБВГДЕ", buf);
}

TEST(str_to_upper_mixed)
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

TEST(str_to_lower_utf)
{
	char str[] = "АбВгДе";
	char buf[sizeof(str)*4];

	assert_success(str_to_lower(str, buf, sizeof(buf)));
	assert_string_equal("абвгде", buf);
}

TEST(str_to_lower_mixed)
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

TEST(str_to_lower_too_short_utf)
{
	char str[] = "абвг";
	char buf[sizeof(str) - 1];

	assert_failure(str_to_lower(str, buf, sizeof(buf)));
	assert_string_equal("абв", buf);
}

TEST(str_to_upper_too_short_utf)
{
	char str[] = "абвг";
	char buf[sizeof(str) - 1];

	assert_failure(str_to_upper(str, buf, sizeof(buf)));
	assert_string_equal("АБВ", buf);
}

TEST(str_to_lower_too_short_mixed)
{
	char str[] = "аaбbвcгd";
	char buf[sizeof(str) - 1];

	assert_failure(str_to_lower(str, buf, sizeof(buf)));
	assert_string_equal("аaбbвcг", buf);
}

TEST(str_to_upper_too_short_mixed)
{
	char str[] = "аaбbвcгd";
	char buf[sizeof(str) - 1];

	assert_failure(str_to_upper(str, buf, sizeof(buf)));
	assert_string_equal("АAБBВCГ", buf);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
