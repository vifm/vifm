#include <stic.h>

#include <stdlib.h> /* free() */

#include "../../src/engine/completion.h"
#include "../../src/engine/options.h"
#include "../../src/engine/text_buffer.h"

static void fastrun_handler(OPT_OP op, optval_t val);
static void fusehome_handler(OPT_OP op, optval_t val);

extern int fastrun;
static int fastrun_local;

SETUP()
{
	optval_t val;

	val.bool_val = fastrun_local = 0;
	vle_opts_add("fastrun", "", "descr", OPT_BOOL, OPT_LOCAL, 0, NULL,
			&fastrun_handler, val);

	val.str_val = "fusehome-default-local";
	vle_opts_add("fusehome", "fh", "descr", OPT_STR, OPT_LOCAL, 0, NULL,
			&fusehome_handler, val);
}

static void
fastrun_handler(OPT_OP op, optval_t val)
{
	fastrun_local = val.bool_val;
}

static void
fusehome_handler(OPT_OP op, optval_t val)
{
}

TEST(global_option_is_set)
{
	const int global = fastrun;
	const int local = fastrun_local;

	assert_success(vle_opts_set("invfastrun", OPT_GLOBAL));
	assert_int_equal(!global, fastrun);
	assert_int_equal(local, fastrun_local);
}

TEST(local_option_is_set)
{
	const int global = fastrun;
	const int local = fastrun_local;

	assert_success(vle_opts_set("invfastrun", OPT_LOCAL));
	assert_int_equal(!local, fastrun_local);
	assert_int_equal(global, fastrun);
}

TEST(global_option_is_printed_on_global)
{
	const char *expected = "nofastrun";

	assert_success(vle_opts_set("invfastrun", OPT_LOCAL));
	assert_int_equal(!fastrun, fastrun_local);

	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("fastrun?", OPT_GLOBAL));
	assert_string_equal(expected, vle_tb_get_data(vle_err));
}

TEST(local_option_is_printed_on_local)
{
	const char *expected = "  fastrun";

	assert_success(vle_opts_set("invfastrun", OPT_LOCAL));
	assert_int_equal(!fastrun, fastrun_local);

	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("fastrun?", OPT_LOCAL));
	assert_string_equal(expected, vle_tb_get_data(vle_err));
}

TEST(local_option_is_printed_on_any)
{
	const char *expected = "  fastrun";

	assert_success(vle_opts_set("invfastrun", OPT_LOCAL));
	assert_int_equal(!fastrun, fastrun_local);

	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("fastrun?", OPT_LOCAL));
	assert_string_equal(expected, vle_tb_get_data(vle_err));
}

TEST(full_local_option_is_not_found_by_global_abbr)
{
	/* This asserts for success because accessing local value of global-only
	 * option silently does nothing. */
	assert_success(vle_opts_set("invfr", OPT_LOCAL));
}

TEST(set_local_reports_unknown_global_options)
{
	assert_failure(vle_opts_set("invsomething", OPT_LOCAL));
}

TEST(get_option_finds_global)
{
	const char *expected = "fusehome-default";
	assert_success(vle_opts_set("fusehome=local-value", OPT_LOCAL));
	assert_string_equal(expected, vle_opts_get("fusehome", OPT_GLOBAL));
}

TEST(get_option_finds_local)
{
	const char *expected = "local-value";
	assert_success(vle_opts_set("fusehome=local-value", OPT_LOCAL));
	assert_string_equal(expected, vle_opts_get("fusehome", OPT_LOCAL));
}

TEST(any_finds_local)
{
	const char *expected = "local-value";
	assert_success(vle_opts_set("fusehome=local-value", OPT_LOCAL));
	assert_string_equal(expected, vle_opts_get("fusehome", OPT_ANY));
}

TEST(any_finds_global)
{
	const char *expected = "name";
	assert_string_equal(expected, vle_opts_get("sort", OPT_ANY));
}

TEST(all_prints_only_local_options)
{
	const char *expected = "  fastrun\n"
	                       "  fusehome=fusehome-default-local";

	assert_success(vle_opts_set("invfastrun", OPT_LOCAL));
	assert_int_equal(!fastrun, fastrun_local);

	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("all", OPT_LOCAL));
	assert_string_equal(expected, vle_tb_get_data(vle_err));
}

TEST(all_prints_only_global_options)
{
	const char *expected = "  cdpath=\n"
	                       "  cpoptions=\n"
	                       "nofastrun\n"
	                       "  fusehome=fusehome-default\n"
	                       "  sort=name\n"
	                       "  sortorder\n"
	                       "  tabstop=8\n"
	                       "  vifminfo=";

	assert_success(vle_opts_set("invfastrun", OPT_LOCAL));
	assert_int_equal(!fastrun, fastrun_local);

	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("all", OPT_GLOBAL));
	assert_string_equal(expected, vle_tb_get_data(vle_err));
}

TEST(all_prints_mixed_options)
{
	const char *expected = "  cdpath=\n"
	                       "  cpoptions=\n"
	                       "  fastrun\n"
	                       "  fusehome=fusehome-default-local\n"
	                       "  sort=name\n"
	                       "  sortorder\n"
	                       "  tabstop=8\n"
	                       "  vifminfo=";

	assert_success(vle_opts_set("invfastrun", OPT_LOCAL));
	assert_int_equal(!fastrun, fastrun_local);

	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("all", OPT_ANY));
	assert_string_equal(expected, vle_tb_get_data(vle_err));
}

TEST(reset_of_all_local_options_preserves_global_options)
{
	assert_success(vle_opts_set("fusehome=global", OPT_GLOBAL));
	assert_success(vle_opts_set("fusehome=local", OPT_LOCAL));
	assert_success(vle_opts_set("all&", OPT_LOCAL));
	assert_success(vle_opts_set("fusehome=global", OPT_GLOBAL));
	assert_success(vle_opts_set("fusehome=fusehome-default-local", OPT_LOCAL));
}

TEST(reset_of_all_global_options_preserves_local_options)
{
	assert_success(vle_opts_set("fusehome=global", OPT_GLOBAL));
	assert_success(vle_opts_set("fusehome=local", OPT_LOCAL));
	assert_success(vle_opts_set("all&", OPT_GLOBAL));
	assert_success(vle_opts_set("fusehome=fusehome-default", OPT_GLOBAL));
	assert_success(vle_opts_set("fusehome=local", OPT_LOCAL));
}

TEST(reset_of_all_options_preserves_local_options)
{
	assert_success(vle_opts_set("fusehome=global", OPT_GLOBAL));
	assert_success(vle_opts_set("fusehome=local", OPT_LOCAL));
	assert_success(vle_opts_set("all&", OPT_ANY));
	assert_success(vle_opts_set("fusehome=fusehome-default", OPT_GLOBAL));
	assert_success(vle_opts_set("fusehome=fusehome-default-local", OPT_LOCAL));
}

TEST(local_only_completion)
{
	const char *start;
	char *completed;

	vle_compl_reset();
	vle_opts_complete("", &start, OPT_LOCAL);

	completed = vle_compl_next();
	assert_string_equal("all", completed);
	free(completed);

	completed = vle_compl_next();
	assert_string_equal("fastrun", completed);
	free(completed);

	completed = vle_compl_next();
	assert_string_equal("fusehome", completed);
	free(completed);
}

TEST(print_only_global_changed_options)
{
	const char *expected = "  fastrun\n"
	                       "  fusehome=global";

	assert_success(vle_opts_set("invfastrun", OPT_LOCAL));
	assert_success(vle_opts_set("invfastrun", OPT_GLOBAL));
	assert_success(vle_opts_set("fusehome=local", OPT_LOCAL));
	assert_success(vle_opts_set("fusehome=global", OPT_GLOBAL));

	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("", OPT_GLOBAL));
	assert_string_equal(expected, vle_tb_get_data(vle_err));
}

TEST(print_only_local_changed_options)
{
	const char *expected = "  fastrun\n"
	                       "  fusehome=local";

	assert_success(vle_opts_set("invfastrun", OPT_LOCAL));
	assert_success(vle_opts_set("invfastrun", OPT_GLOBAL));
	assert_success(vle_opts_set("fusehome=local", OPT_LOCAL));
	assert_success(vle_opts_set("fusehome=global", OPT_GLOBAL));

	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("", OPT_LOCAL));
	assert_string_equal(expected, vle_tb_get_data(vle_err));
}

TEST(print_any_changed_options)
{
	const char *expected = "  fastrun\n"
	                       "  fusehome=local";

	assert_success(vle_opts_set("invfastrun", OPT_LOCAL));
	assert_success(vle_opts_set("invfastrun", OPT_GLOBAL));
	assert_success(vle_opts_set("fusehome=local", OPT_LOCAL));
	assert_success(vle_opts_set("fusehome=global", OPT_GLOBAL));

	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("", OPT_ANY));
	assert_string_equal(expected, vle_tb_get_data(vle_err));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
