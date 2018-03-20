#include <stic.h>

#include <stdlib.h>

#include "../../src/engine/variables.h"
#include "../../src/utils/utils.h"

#define VAR_A               "VAR_A"
#define VAR_A_VAL           "VA%R_ _VAL"
#define VAR_A_ESCAPED_VAL   "VA%R_\\ _VAL"
#define VAR_B               "VAR_B"
#define VAR_B_VAL           "VAR_B_VAL"

SETUP()
{
	init_variables();
	let_variables("$" VAR_A " = '" VAR_A_VAL "'");
	let_variables("$" VAR_B " = '" VAR_B_VAL "'");
}

TEARDOWN()
{
	clear_envvars();
}

TEST(empty_string_ok)
{
	const char *const path = "";
	char *const expanded_path = expand_envvars(path, 1);
	assert_string_equal("", expanded_path);
	free(expanded_path);
}

TEST(no_envvars_ok)
{
	const char *const path = "/usr/bin/vifm";
	char *const expanded_path = expand_envvars(path, 1);
	assert_string_equal(path, expanded_path);
	free(expanded_path);
}

TEST(expanding_works)
{
	const char *const path = "/$" VAR_A "/$" VAR_B "/vifm";
	char *const expanded_path = expand_envvars(path, 1);
	assert_string_equal("/" VAR_A_ESCAPED_VAL "/" VAR_B_VAL "/vifm",
			expanded_path);
	free(expanded_path);
}

TEST(ends_with_dollar_sign_ok)
{
	const char *const path = "/usr/bin/vifm$";
	char *const expanded_path = expand_envvars(path, 1);
	assert_string_equal(path, expanded_path);
	free(expanded_path);
}

TEST(double_dollar_sign_not_folded)
{
	const char *const path = "/usr/b$$" VAR_A "/vifm";
	char *const expanded_path = expand_envvars(path, 1);
	assert_string_equal("/usr/b$" VAR_A_ESCAPED_VAL "/vifm", expanded_path);
	free(expanded_path);
}

TEST(escaped_dollar_sign_not_expanded)
{
	const char *const path = "/usr/b\\$in/vifm";
	char *const expanded_path = expand_envvars(path, 1);
	assert_string_equal("/usr/b\\$in/vifm", expanded_path);
	free(expanded_path);
}

TEST(no_escaping_works)
{
	const char *const path = "/$" VAR_A "/$" VAR_B "/vifm";
	char *const expanded_path = expand_envvars(path, 0);
	assert_string_equal("/" VAR_A_VAL "/" VAR_B_VAL "/vifm",
			expanded_path);
	free(expanded_path);
}

TEST(escaped_dollar_sign_folded)
{
	const char *const path = "/usr/b\\$in/vifm";
	char *const expanded_path = expand_envvars(path, 0);
	assert_string_equal("/usr/b$in/vifm", expanded_path);
	free(expanded_path);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
