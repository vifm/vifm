#include <stic.h>

#include <stdlib.h>

#include "../../src/engine/completion.h"
#include "../../src/engine/variables.h"

TEST(vars_empty_completion)
{
	char buf[] = "";
	const char *start;
	char *completed;

	complete_variables(buf, &start);
	assert_true(&buf[0] == start);
	completed = vle_compl_next();
	assert_string_equal("", completed);
	free(completed);
}

TEST(vars_completion)
{
	char buf[] = "abc";
	const char *start;
	char *completed;

	complete_variables(buf, &start);
	assert_true(&buf[0] == start);
	completed = vle_compl_next();
	assert_string_equal("abc", completed);
	free(completed);
}

TEST(envvars_completion)
{
	char buf[] = "$VAR_";
	const char *start;
	char *completed;

	complete_variables(buf, &start);
	assert_true(&buf[1] == start);

	completed = vle_compl_next();
	assert_string_equal("VAR_A", completed);
	free(completed);

	completed = vle_compl_next();
	assert_string_equal("VAR_B", completed);
	free(completed);

	completed = vle_compl_next();
	assert_string_equal("VAR_C", completed);
	free(completed);

	completed = vle_compl_next();
	assert_string_equal("VAR_", completed);
	free(completed);
}

TEST(do_not_complete_removed_variables)
{
	char buf[] = "$VAR_";
	const char *start;
	char *completed;

	assert_int_equal(0, unlet_variables("$VAR_B"));

	complete_variables(buf, &start);
	assert_true(&buf[1] == start);

	completed = vle_compl_next();
	assert_string_equal("VAR_A", completed);
	free(completed);

	completed = vle_compl_next();
	assert_string_equal("VAR_C", completed);
	free(completed);

	completed = vle_compl_next();
	assert_string_equal("VAR_", completed);
	free(completed);
}

TEST(builtinvars_completion)
{
	char buf[] = "v:";
	const char *start;
	char *completed;

	assert_success(setvar("v:test1", var_from_bool(1)));
	assert_success(setvar("v:test2", var_from_bool(1)));

	complete_variables(buf, &start);
	assert_string_equal(&buf[0], start);

	completed = vle_compl_next();
	assert_string_equal("v:test1", completed);
	free(completed);

	completed = vle_compl_next();
	assert_string_equal("v:test2", completed);
	free(completed);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
