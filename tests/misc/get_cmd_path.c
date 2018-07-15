#include <stic.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/engine/variables.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_completion.h"

SETUP()
{
	init_variables();
}

TEARDOWN()
{
	clear_variables();
}

TEST(emarks_are_skipped)
{
	char path[PATH_MAX + 1];
	assert_success(get_cmd_path("!!/prog", sizeof(path), path));
	assert_string_equal("/prog", path);
}

TEST(tilde_is_expanded)
{
	char path[PATH_MAX + 1];
	copy_str(cfg.home_dir, sizeof(cfg.home_dir), "tilde_is_expanded/");
	assert_success(get_cmd_path("~/prog", sizeof(path), path));
	assert_string_equal("tilde_is_expanded/prog", path);
}

TEST(envvars_are_expanded)
{
	char path[PATH_MAX + 1];
	let_variables("$TEST_ENVVAR = 'envvars_are_expanded'");
	assert_success(get_cmd_path("$TEST_ENVVAR/prog", sizeof(path), path));
	assert_string_equal("envvars_are_expanded/prog", path);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
