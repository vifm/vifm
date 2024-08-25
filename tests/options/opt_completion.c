#include <stic.h>

#include <stdlib.h>

#include "../../src/engine/completion.h"
#include "../../src/engine/options.h"

#define ASSERT_NEXT_MATCH(str) \
	do \
	{ \
		char *const buf = vle_compl_next(); \
		assert_string_equal((str), buf); \
		free(buf); \
	} \
	while(0)

TEST(space_at_the_end)
{
	const char *start;

	vle_compl_reset();
	vle_opts_complete("fusehome=a\\ b\\ ", &start, OPT_GLOBAL);
	ASSERT_NEXT_MATCH("a\\ b\\ ");

	vle_compl_reset();
	vle_opts_complete("fusehome=a\\ b ", &start, OPT_GLOBAL);
	ASSERT_NEXT_MATCH("all");
	ASSERT_NEXT_MATCH("cdpath");
	ASSERT_NEXT_MATCH("cpoptions");
	ASSERT_NEXT_MATCH("fastrun");
}

TEST(one_choice_opt)
{
	const char *start;

	vle_compl_reset();
	vle_opts_complete("fuse", &start, OPT_GLOBAL);
	ASSERT_NEXT_MATCH("fusehome");
	ASSERT_NEXT_MATCH("fusehome");
	ASSERT_NEXT_MATCH("fusehome");
}

TEST(one_choice_val)
{
	char buf[] = "sort=n";
	const char *start;

	vle_compl_reset();
	vle_opts_complete(buf, &start, OPT_GLOBAL);
	assert_true(start == buf + 5);
	ASSERT_NEXT_MATCH("name");
	ASSERT_NEXT_MATCH("name");
	ASSERT_NEXT_MATCH("name");
}

TEST(invalid_input)
{
	const char *start;

	vle_compl_reset();
	vle_opts_complete("fast ?f", &start, OPT_GLOBAL);
	ASSERT_NEXT_MATCH("fast ?f");
}

TEST(skip_abbreviations)
{
	const char *start;

	vle_compl_reset();
	vle_opts_complete("", &start, OPT_GLOBAL);

	ASSERT_NEXT_MATCH("all");
	ASSERT_NEXT_MATCH("cdpath");
	ASSERT_NEXT_MATCH("cpoptions");
	ASSERT_NEXT_MATCH("fastrun");
	ASSERT_NEXT_MATCH("fusehome");
	ASSERT_NEXT_MATCH("sort");
	ASSERT_NEXT_MATCH("sortorder");
	ASSERT_NEXT_MATCH("tabstop");
	ASSERT_NEXT_MATCH("vifminfo");
	ASSERT_NEXT_MATCH("");
}

TEST(expand_abbreviations)
{
	const char *start;

	vle_compl_reset();
	vle_opts_complete("fr", &start, OPT_GLOBAL);
	ASSERT_NEXT_MATCH("fastrun");
	ASSERT_NEXT_MATCH("fastrun");
}

TEST(after_eq_value_completion)
{
	char buf[] = "vifminfo=op";
	const char *start;

	vle_compl_reset();
	vle_opts_complete(buf, &start, OPT_GLOBAL);
	assert_true(start == buf + 9);
	ASSERT_NEXT_MATCH("options");
	ASSERT_NEXT_MATCH("options");
}

TEST(after_meq_value_completion)
{
	char buf[] = "vifminfo-=op";
	const char *start;

	vle_compl_reset();
	vle_opts_complete(buf, &start, OPT_GLOBAL);
	assert_true(start == buf + 10);
	ASSERT_NEXT_MATCH("options");
	ASSERT_NEXT_MATCH("options");
}

TEST(after_peq_value_completion)
{
	char buf[] = "vifminfo+=op";
	const char *start;

	vle_compl_reset();
	vle_opts_complete(buf, &start, OPT_GLOBAL);
	assert_true(start == buf + 10);
	ASSERT_NEXT_MATCH("options");
	ASSERT_NEXT_MATCH("options");
}

TEST(set_values_completion)
{
	char buf[] = "vifminfo=tui,c";
	const char *start;

	vle_compl_reset();
	vle_opts_complete(buf, &start, OPT_GLOBAL);
	assert_true(start == buf + 13);
	ASSERT_NEXT_MATCH("commands");
	ASSERT_NEXT_MATCH("cs");
	ASSERT_NEXT_MATCH("c");
}

TEST(colon)
{
	const char *start;

	vle_compl_reset();
	vle_opts_complete("fusehome:a\\ b\\ ", &start, OPT_GLOBAL);
	ASSERT_NEXT_MATCH("a\\ b\\ ");

	vle_compl_reset();
	vle_opts_complete("fusehome:a\\ b ", &start, OPT_GLOBAL);
	ASSERT_NEXT_MATCH("all");
	ASSERT_NEXT_MATCH("cdpath");
	ASSERT_NEXT_MATCH("cpoptions");
	ASSERT_NEXT_MATCH("fastrun");
}

TEST(umbiguous_beginning)
{
	const char *start;

	vle_compl_reset();
	vle_opts_complete("sort", &start, OPT_GLOBAL);
	ASSERT_NEXT_MATCH("sort");
	ASSERT_NEXT_MATCH("sortorder");
}

TEST(matching_short_full)
{
	const char *start;

	vle_compl_reset();
	vle_opts_complete("so", &start, OPT_GLOBAL);
	ASSERT_NEXT_MATCH("sort");
	ASSERT_NEXT_MATCH("sortorder");
	ASSERT_NEXT_MATCH("so");
}

TEST(after_equal_sign_completion_ok)
{
	const char *start;

	optval_t val = { .str_val = "/home/tmp" };
	vle_opts_assign("fusehome", val, OPT_GLOBAL);

	vle_compl_reset();
	vle_opts_complete("fusehome=", &start, OPT_GLOBAL);
	ASSERT_NEXT_MATCH("/home/tmp");
}

TEST(after_equal_sign_completion_spaces_ok)
{
	const char *start;

	optval_t val = { .str_val = "/home directory/tmp" };
	vle_opts_assign("fusehome", val, OPT_GLOBAL);

	vle_compl_reset();
	vle_opts_complete("fusehome=", &start, OPT_GLOBAL);
	ASSERT_NEXT_MATCH("/home\\ directory/tmp");
}

TEST(after_fake_equal_sign_completion_fail)
{
	const char *start;

	optval_t val = { .str_val = "/home/tmp" };
	vle_opts_assign("fusehome", val, OPT_GLOBAL);

	vle_compl_reset();
	vle_opts_complete("fusehome=a=", &start, OPT_GLOBAL);
	ASSERT_NEXT_MATCH("a=");
}

TEST(after_equal_sign_and_comma_completion_fail)
{
	const char *start;

	optval_t val = { .str_val = "/home/tmp" };
	vle_opts_assign("cdpath", val, OPT_GLOBAL);

	vle_compl_reset();
	vle_opts_complete("cdpath=a,", &start, OPT_GLOBAL);
	ASSERT_NEXT_MATCH("a,");
}

TEST(all_completion_ok)
{
	const char *start;

	vle_compl_reset();
	vle_opts_complete("all=", &start, OPT_GLOBAL);
	ASSERT_NEXT_MATCH("");
}

TEST(completion_of_second_option)
{
	const char *str = "all no";
	const char *start;

	vle_compl_reset();
	vle_opts_complete(str, &start, OPT_GLOBAL);
	assert_string_equal(str + 6, start);
	ASSERT_NEXT_MATCH("fastrun");
	ASSERT_NEXT_MATCH("sortorder");
}

TEST(charset_completion_skips_entered_elements)
{
	const char *start;

	vle_compl_reset();
	vle_opts_complete("cpo=a", &start, OPT_GLOBAL);
	ASSERT_NEXT_MATCH("b");
	ASSERT_NEXT_MATCH("c");
	ASSERT_NEXT_MATCH("");
	ASSERT_NEXT_MATCH("b");
}

TEST(charset_hat_completion_works)
{
	const char *start;

	vle_compl_reset();
	vle_opts_complete("cpo^=a", &start, OPT_GLOBAL);
	ASSERT_NEXT_MATCH("b");
	ASSERT_NEXT_MATCH("c");
	ASSERT_NEXT_MATCH("");
	ASSERT_NEXT_MATCH("b");
}

TEST(no_no_or_inv_completion_for_all_pseudo_option)
{
	const char *start;

	vle_compl_reset();
	vle_opts_complete("noa", &start, OPT_GLOBAL);
	assert_string_equal("a", start);
	ASSERT_NEXT_MATCH("a");
	ASSERT_NEXT_MATCH("a");

	vle_compl_reset();
	vle_opts_complete("inva", &start, OPT_GLOBAL);
	assert_string_equal("a", start);
	ASSERT_NEXT_MATCH("a");
	ASSERT_NEXT_MATCH("a");
}

TEST(uncompleteable_args_are_ignored)
{
	const char *start;

	vle_compl_reset();
	vle_opts_complete("'bla", &start, OPT_GLOBAL);
	assert_string_equal("'bla", start);
	ASSERT_NEXT_MATCH("'bla");
	ASSERT_NEXT_MATCH("'bla");

	vle_compl_reset();
	vle_opts_complete("\"bla", &start, OPT_GLOBAL);
	assert_string_equal("\"bla", start);
	ASSERT_NEXT_MATCH("\"bla");
	ASSERT_NEXT_MATCH("\"bla");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
