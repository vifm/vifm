#include <stic.h>

#include "../../src/cfg/config.h"
#include "../../src/color_scheme.h"
#include "../../src/commands.h"
#include "../../src/status.h"

SETUP_ONCE()
{
	init_commands();
	reset_color_scheme(&cfg.cs);
	lwin.list_rows = 0;
	rwin.list_rows = 0;
}

SETUP()
{
	reset_color_scheme(&cfg.cs);
	curr_stats.cs = &cfg.cs;
}

TEST(empty_curly_braces)
{
	const char *const COMMANDS = "highlight {} ctermfg=red";

	assert_false(exec_commands(COMMANDS, &lwin, CIT_COMMAND) == 0);
}

TEST(curly_braces_pattern_transform)
{
	const char *const COMMANDS = "highlight {*.sh} ctermfg=red";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal("*.sh", cfg.cs.file_hi[0].pattern);
}

TEST(curly_braces_no_flags_allowed)
{
	const char *const COMMANDS = "highlight {*.sh}i ctermfg=red";

	assert_false(exec_commands(COMMANDS, &lwin, CIT_COMMAND) == 0);
}

TEST(empty_re_without_flags)
{
	const char *const COMMANDS = "highlight // ctermfg=red";

	assert_false(exec_commands(COMMANDS, &lwin, CIT_COMMAND) == 0);
}

TEST(empty_re_with_flags)
{
	const char *const COMMANDS = "highlight //i ctermfg=red";

	assert_false(exec_commands(COMMANDS, &lwin, CIT_COMMAND) == 0);
}

TEST(pattern_is_not_unescaped)
{
	const char *const COMMANDS = "highlight /^\\./ ctermfg=red";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal("^\\.", cfg.cs.file_hi[0].pattern);
}

TEST(pattern_length_is_not_limited)
{
	const char *const COMMANDS = "highlight /\\.(7z|Z|a|ace|alz|apkg|arc|arj|bz"
		"|bz2|cab|cpio|deb|gz|jar|lha|lrz|lz|lzma|lzo|rar|rpm|rz|t7z|tZ|tar|tbz"
		"|tbz2|tgz|tlz|txz|tzo|war|xz|zip)$/ ctermfg=red";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal("\\.(7z|Z|a|ace|alz|apkg|arc|arj|bz"
		"|bz2|cab|cpio|deb|gz|jar|lha|lrz|lz|lzma|lzo|rar|rpm|rz|t7z|tZ|tar|tbz"
		"|tbz2|tgz|tlz|txz|tzo|war|xz|zip)$", cfg.cs.file_hi[0].pattern);
}

TEST(i_flag)
{
	const char *const COMMANDS = "highlight /^\\./i ctermfg=red";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal("^\\.", cfg.cs.file_hi[0].pattern);
}

TEST(I_flag)
{
	const char *const COMMANDS = "highlight /^\\./I ctermfg=red";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal("^\\.", cfg.cs.file_hi[0].pattern);
}

TEST(wrong_flag)
{
	const char *const COMMANDS = "highlight /^\\./x ctermfg=red";

	assert_int_equal(1, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
