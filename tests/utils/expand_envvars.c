#include <stic.h>

#include <stdlib.h>

#include "../../src/engine/variables.h"
#include "../../src/utils/utils.h"

#define VAR_A                                 "VAR_A"
#define VAR_A_VAL                             "VA%R_ _VAL"
#define VAR_A_ESCAPED_VAL                     "VA%R_\\ _VAL"
#define VAR_A_DOUBLED_PERCENT_VAL             "VA%%R_ _VAL"
#define VAR_A_ESCAPED_AND_DOUBLED_PERCENT_VAL "VA%%R_\\ _VAL"
#define VAR_B                                 "VAR_B"
#define VAR_B_VAL                             "VAR_B_VAL"
#define VAR_NON_EXISTENT                      "VAR_NON_EXISTENT"

SETUP()
{
	init_variables();
	let_variables("$" VAR_A " = '" VAR_A_VAL "'");
	let_variables("$" VAR_B " = '" VAR_B_VAL "'");
	let_variables("$" VAR_NON_EXISTENT " = ''");
	unlet_variables("$" VAR_NON_EXISTENT);
}

TEARDOWN()
{
	clear_envvars();
}

TEST(empty_string_ok)
{
	const char *const path = "";
	char *const expanded_path =
		expand_envvars(path, EEF_ESCAPE_VALS | EEF_KEEP_ESCAPES);
	assert_string_equal("", expanded_path);
	free(expanded_path);
}

TEST(no_envvars_ok)
{
	const char *const path = "/usr/bin/vifm";
	char *const expanded_path =
		expand_envvars(path, EEF_ESCAPE_VALS | EEF_KEEP_ESCAPES);
	assert_string_equal(path, expanded_path);
	free(expanded_path);
}

TEST(expanding_works)
{
	const char *const path = "/$" VAR_A "/$" VAR_B "/vifm";
	char *const expanded_path =
		expand_envvars(path, EEF_ESCAPE_VALS | EEF_KEEP_ESCAPES);
	assert_string_equal("/" VAR_A_ESCAPED_VAL "/" VAR_B_VAL "/vifm",
			expanded_path);
	free(expanded_path);
}

TEST(ends_with_dollar_sign_ok)
{
	const char *const path = "/usr/bin/vifm$";
	char *const expanded_path =
		expand_envvars(path, EEF_ESCAPE_VALS | EEF_KEEP_ESCAPES);
	assert_string_equal(path, expanded_path);
	free(expanded_path);
}

TEST(double_dollar_sign_not_folded)
{
	const char *const path = "/usr/b$$" VAR_A "/vifm";
	char *const expanded_path =
		expand_envvars(path, EEF_ESCAPE_VALS | EEF_KEEP_ESCAPES);
	assert_string_equal("/usr/b$" VAR_A_ESCAPED_VAL "/vifm", expanded_path);
	free(expanded_path);
}

TEST(dollar_sign_not_truncated)
{
	const char *const path = "$" VAR_NON_EXISTENT;
	char *const expanded_path = expand_envvars(path, EEF_ESCAPE_VALS);
	assert_string_equal(path, expanded_path);
	free(expanded_path);
}

TEST(escaped_dollar_sign_not_expanded)
{
	const char *const path = "/usr/b\\$in/vifm";
	char *const expanded_path =
		expand_envvars(path, EEF_ESCAPE_VALS | EEF_KEEP_ESCAPES);
	assert_string_equal("/usr/b$in/vifm", expanded_path);
	free(expanded_path);
}

TEST(no_escaping_works)
{
	const char *const path = "/$" VAR_A "/$" VAR_B "/vifm";
	char *const expanded_path = expand_envvars(path, EEF_NONE);
	assert_string_equal("/" VAR_A_VAL "/" VAR_B_VAL "/vifm", expanded_path);
	free(expanded_path);
}

TEST(escaped_dollar_sign_folded)
{
	const char *const path = "/usr/b\\$in/vifm";
	char *const expanded_path = expand_envvars(path, EEF_NONE);
	assert_string_equal("/usr/b$in/vifm", expanded_path);
	free(expanded_path);
}

TEST(percent_not_doubled_outside_of_value)
{
	const char *const path = "/%/";
	char *const expanded_path = expand_envvars(path, EEF_DOUBLE_PERCENTS);
	assert_string_equal(path, expanded_path);
	free(expanded_path);
}

TEST(doubled_percent)
{
	const char *const path = "$" VAR_A;
	char *const expanded_path = expand_envvars(path, EEF_DOUBLE_PERCENTS);
	assert_string_equal(VAR_A_DOUBLED_PERCENT_VAL, expanded_path);
	free(expanded_path);
}

TEST(escaped_and_doubled_percent)
{
	const char *const path = "$" VAR_A;
	char *const expanded_path = expand_envvars(path,
			EEF_ESCAPE_VALS | EEF_DOUBLE_PERCENTS);
	assert_string_equal(VAR_A_ESCAPED_AND_DOUBLED_PERCENT_VAL, expanded_path);
	free(expanded_path);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
