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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
