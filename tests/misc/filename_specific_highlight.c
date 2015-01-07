#include "seatest.h"

#include "../../src/cfg/config.h"
#include "../../src/color_scheme.h"
#include "../../src/commands.h"
#include "../../src/status.h"

static void
setup(void)
{
	reset_color_scheme(&cfg.cs);
	curr_stats.cs = &cfg.cs;
}

static void
test_pattern_is_not_unescaped(void)
{
	const char *const COMMANDS = "highlight /^\\./ ctermfg=red";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, GET_COMMAND));
	assert_string_equal("^\\.", cfg.cs.file_hi[0].pattern);
}

static void
test_pattern_length_is_not_limited(void)
{
	const char *const COMMANDS = "highlight /\\.(7z|Z|a|ace|alz|apkg|arc|arj|bz"
		"|bz2|cab|cpio|deb|gz|jar|lha|lrz|lz|lzma|lzo|rar|rpm|rz|t7z|tZ|tar|tbz"
		"|tbz2|tgz|tlz|txz|tzo|war|xz|zip)$/ ctermfg=red";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, GET_COMMAND));
	assert_string_equal("\\.(7z|Z|a|ace|alz|apkg|arc|arj|bz"
		"|bz2|cab|cpio|deb|gz|jar|lha|lrz|lz|lzma|lzo|rar|rpm|rz|t7z|tZ|tar|tbz"
		"|tbz2|tgz|tlz|txz|tzo|war|xz|zip)$", cfg.cs.file_hi[0].pattern);
}

void
filename_specific_highlight_tests(void)
{
	test_fixture_start();

	init_commands();
	reset_color_scheme(&cfg.cs);

	fixture_setup(setup);

	run_test(test_pattern_is_not_unescaped);
	run_test(test_pattern_length_is_not_limited);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
