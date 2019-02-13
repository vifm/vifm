#include <stic.h>

#include <stdlib.h>

#include "../../src/engine/options.h"
#include "../../src/engine/variables.h"

static void fusehome_handler(OPT_OP op, optval_t val);
static void number_handler(OPT_OP op, optval_t val);
static void numberwidth_global(OPT_OP op, optval_t val);
static void numberwidth_local(OPT_OP op, optval_t val);

static const char *fuse_home;
static int number;
static int number_width_global;
static int number_width_local;

SETUP()
{
	static int option_changed;
	optval_t def = { .str_val = "/tmp" };

	vle_opts_init(&option_changed, NULL);

	def.str_val = "/tmp";
	vle_opts_add("fusehome", "fh", "descr", OPT_STR, OPT_GLOBAL, 0, NULL,
			&fusehome_handler, def);

	def.int_val = 0;
	vle_opts_add("number", "nu", "descr", OPT_BOOL, OPT_GLOBAL, 0, NULL,
			&number_handler, def);
	vle_opts_add("numberwidth", "nuw", "descr", OPT_INT, OPT_GLOBAL, 0, NULL,
			&numberwidth_global, def);
	vle_opts_add("numberwidth", "nuw", "descr", OPT_INT, OPT_LOCAL, 0, NULL,
			&numberwidth_local, def);

	fuse_home = NULL;
	number_width_global = 0;
	number_width_local = 0;
}

TEARDOWN()
{
	vle_opts_reset();
}

static void
fusehome_handler(OPT_OP op, optval_t val)
{
	fuse_home = val.str_val;
}

static void
number_handler(OPT_OP op, optval_t val)
{
	number = val.bool_val;
}

static void
numberwidth_global(OPT_OP op, optval_t val)
{
	number_width_global = val.int_val;
}

static void
numberwidth_local(OPT_OP op, optval_t val)
{
	number_width_local = val.int_val;
}

TEST(set_global_option_without_prefix)
{
	assert_success(let_variables("&fusehome = 'value'"));
	assert_string_equal("value", fuse_home);
}

TEST(set_global_option_with_global_prefix)
{
	assert_success(let_variables("&g:fusehome = 'value'"));
	assert_string_equal("value", fuse_home);
}

TEST(set_global_option_with_local_prefix)
{
	assert_failure(let_variables("&l:fusehome = 'value'"));
	assert_string_equal(NULL, fuse_home);
}

TEST(set_global_and_local_option_without_prefix)
{
	assert_success(let_variables("&numberwidth = 2"));
	assert_int_equal(2, number_width_global);
	assert_int_equal(2, number_width_local);
}

TEST(set_global_and_local_option_with_global_prefix)
{
	assert_success(let_variables("&g:numberwidth = 2"));
	assert_int_equal(2, number_width_global);
	assert_int_equal(0, number_width_local);
}

TEST(set_global_and_local_option_with_local_prefix)
{
	assert_success(let_variables("&l:numberwidth = 2"));
	assert_int_equal(0, number_width_global);
	assert_int_equal(2, number_width_local);
}

TEST(appending_works_for_strings)
{
	assert_success(let_variables("&fusehome = 'value'"));
	assert_string_equal("value", fuse_home);
	assert_success(let_variables("&fusehome .= 'more'"));
	assert_string_equal("valuemore", fuse_home);
}

TEST(incdec_does_not_work_for_strings)
{
	assert_failure(let_variables("&fusehome += 3"));
	assert_failure(let_variables("&fusehome -= 3"));
}

TEST(incdec_works_for_numbers)
{
	assert_success(let_variables("&numberwidth += 2"));
	assert_int_equal(2, number_width_local);
	assert_success(let_variables("&numberwidth += 4"));
	assert_int_equal(6, number_width_local);
	assert_success(let_variables("&numberwidth -= 5"));
	assert_int_equal(1, number_width_local);
}

TEST(appending_does_not_work_for_numbers)
{
	assert_failure(let_variables("&numberwidth .= 2"));
}

TEST(appending_does_not_work_for_bools)
{
	assert_failure(let_variables("&number .= 2"));
}

TEST(addition_does_not_work_for_bools)
{
	assert_failure(let_variables("&number += 2"));
}

#if 0

/* For the future. */
TEST(set_of_a_boolean_option)
{
	assert_failure(let_variables("&number = 'a'"));

	assert_success(let_variables("&number = '1'"));
	assert_true(number);
	assert_success(let_variables("&number = '0'"));
	assert_false(number);
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
