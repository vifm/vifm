#include <stic.h>

#include "../../src/cfg/config.h"
#include "../../src/utils/str.h"
#include "../../src/utils/utils.h"

SETUP()
{
	cfg.sizefmt.ieci_prefixes = 0;
	cfg.sizefmt.base = 1024;
	cfg.sizefmt.precision = 0;
	cfg.sizefmt.space = 1;
}

TEST(removing_useless_trailing_zero)
{
	char buf[16];

	friendly_size_notation(4*1024, sizeof(buf), buf);
	assert_string_equal("4 K", buf);

	friendly_size_notation(1024*1024 - 1, sizeof(buf), buf);
	assert_string_equal("1 M", buf);
}

TEST(problem_1024)
{
	char buf[16];

	friendly_size_notation(1024*1024 - 2, sizeof(buf), buf);
	assert_string_equal("1 M", buf);
}

TEST(from_zero_rounding)
{
	char buf[16];

	friendly_size_notation(8*1024 - 1, sizeof(buf), buf);
	assert_string_equal("8 K", buf);

	friendly_size_notation(8*1024 + 1, sizeof(buf), buf);
	assert_string_equal("8.1 K", buf);
}

TEST(iso_2)
{
	char buf[16];

	cfg.sizefmt.base = 1000;
	cfg.sizefmt.precision = 2;

	friendly_size_notation(111, sizeof(buf), buf);
	assert_string_equal("111  B", buf);

	friendly_size_notation(1000, sizeof(buf), buf);
	assert_string_equal("1 KB", buf);

	friendly_size_notation(1030, sizeof(buf), buf);
	assert_string_equal("1.03 KB", buf);

	friendly_size_notation(1030000, sizeof(buf), buf);
	assert_string_equal("1.03 MB", buf);

	friendly_size_notation(10301024, sizeof(buf), buf);
	assert_string_equal("10.31 MB", buf);
}

TEST(iec_2)
{
	char buf[16];

	cfg.sizefmt.precision = 2;
	cfg.sizefmt.ieci_prefixes = 1;

	friendly_size_notation(111, sizeof(buf), buf);
	assert_string_equal("111   B", buf);

	friendly_size_notation(1000, sizeof(buf), buf);
	assert_string_equal("1000   B", buf);

	friendly_size_notation(1030, sizeof(buf), buf);
	assert_string_equal("1.01 KiB", buf);

	friendly_size_notation(1030000, sizeof(buf), buf);
	assert_string_equal("1005.86 KiB", buf);

	friendly_size_notation(10301024, sizeof(buf), buf);
	assert_string_equal("9.83 MiB", buf);
}

TEST(iec_3)
{
	char buf[16];

	cfg.sizefmt.precision = 3;

	friendly_size_notation(111, sizeof(buf), buf);
	assert_string_equal("111 B", buf);

	friendly_size_notation(1000, sizeof(buf), buf);
	assert_string_equal("1000 B", buf);

	friendly_size_notation(1030, sizeof(buf), buf);
	assert_string_equal("1.006 K", buf);

	friendly_size_notation(1030000, sizeof(buf), buf);
	assert_string_equal("1005.86 K", buf);

	friendly_size_notation(10301024, sizeof(buf), buf);
	assert_string_equal("9.824 M", buf);
}

TEST(huge_precision)
{
	char buf[64];

	cfg.sizefmt.precision = 10;
	friendly_size_notation(231093, sizeof(buf), buf);
	assert_string_equal("225.6767578125 K", buf);

	/* Compiler optimizes operations somewhat differently, so don't expect an
	 * exact match. */
	cfg.sizefmt.precision = 25;
	friendly_size_notation(231093, sizeof(buf), buf);
	assert_true(starts_with_lit(buf, "225.676757812"));
	assert_true(ends_with(buf, " K"));
}

TEST(nospace)
{
	char buf[16];

	cfg.sizefmt.ieci_prefixes = 1;
	cfg.sizefmt.precision = 0;
	cfg.sizefmt.space = 0;

	friendly_size_notation(10301024, sizeof(buf), buf);
	assert_string_equal("10MiB", buf);
}

TEST(nospace_with_precision)
{
	char buf[16];

	cfg.sizefmt.ieci_prefixes = 1;
	cfg.sizefmt.precision = 2;
	cfg.sizefmt.space = 0;

	friendly_size_notation(10301024, sizeof(buf), buf);
	assert_string_equal("9.83MiB", buf);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
