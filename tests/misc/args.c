#include <stic.h>

#include "../../src/utils/macros.h"
#include "../../src/args.h"

TEST(chooseopt_options_are_not_set)
{
	args_t args = { };
	char *argv[] = { "vifm", "+", NULL };

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");

	assert_int_equal(1, args.ncmds);
	assert_string_equal("$", args.cmds[0]);

	args_free(&args);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
