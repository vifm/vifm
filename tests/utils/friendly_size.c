#include <stic.h>

#include "../../src/cfg/config.h"
#include "../../src/utils/utils.h"

SETUP_ONCE()
{
	cfg.use_iec_prefixes = 0;
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
