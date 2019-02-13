#include <stic.h>

#include "../../src/engine/options.h"
#include "../../src/engine/text_buffer.h"

extern char cpoptions[10];
extern int cpoptions_handler_calls;

TEST(assignment_to_something_calls_handler_only_once)
{
	int res;

	cpoptions[0] = '\0';

	cpoptions_handler_calls = 0;
	res = vle_opts_set("cpoptions=abc", OPT_GLOBAL);
	assert_int_equal(1, cpoptions_handler_calls);
	assert_int_equal(0, res);
	assert_string_equal("abc", cpoptions);
}

TEST(assignment_to_something)
{
	int res;

	cpoptions[0] = '\0';

	res = vle_opts_set("cpoptions=ac", OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_string_equal("ac", cpoptions);
}

TEST(assignment_to_same_handler_not_called)
{
	int res;

	cpoptions[0] = '\0';

	res = vle_opts_set("cpoptions=ac", OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_string_equal("ac", cpoptions);

	cpoptions_handler_calls = 0;
	res = vle_opts_set("cpoptions=ac", OPT_GLOBAL);
	assert_int_equal(0, cpoptions_handler_calls);
	assert_int_equal(0, res);
}

TEST(assignment_to_empty)
{
	int res;

	res = vle_opts_set("cpoptions=ac", OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_string_equal("ac", cpoptions);

	res = vle_opts_set("cpoptions=", OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_string_equal("", cpoptions);
}

TEST(char_addition)
{
	int res;

	res = vle_opts_set("cpoptions=ac", OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_string_equal("ac", cpoptions);

	res = vle_opts_set("cpoptions+=b", OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_string_equal("acb", cpoptions);
}

TEST(char_removal)
{
	int res;

	res = vle_opts_set("cpoptions=ac", OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_string_equal("ac", cpoptions);

	res = vle_opts_set("cpoptions-=a", OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_string_equal("c", cpoptions);
}

TEST(multiple_char_addition)
{
	int res;

	res = vle_opts_set("cpoptions=b", OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_string_equal("b", cpoptions);

	res = vle_opts_set("cpoptions+=ac", OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_string_equal("bac", cpoptions);
}

TEST(multiple_char_removal)
{
	int res;

	res = vle_opts_set("cpoptions=abc", OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_string_equal("abc", cpoptions);

	res = vle_opts_set("cpoptions-=ac", OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_string_equal("b", cpoptions);
}

TEST(chars_not_from_the_list_are_rejected_on_assignment)
{
	const int res = vle_opts_set("cpoptions=yxz", OPT_GLOBAL);
	assert_false(res == 0);
}

TEST(chars_not_from_the_list_are_rejected_on_addition)
{
	int res;

	res = vle_opts_set("cpoptions=abc", OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_string_equal("abc", cpoptions);

	res = vle_opts_set("cpoptions+=az", OPT_GLOBAL);
	assert_false(res == 0);
}

TEST(chars_not_from_the_list_are_ignored_on_removal)
{
	int res;

	res = vle_opts_set("cpoptions=abc", OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_string_equal("abc", cpoptions);

	res = vle_opts_set("cpoptions-=xyz", OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_string_equal("abc", cpoptions);
}

TEST(reset_compares_values_as_strings)
{
	int res;

	/* Change value from the default one. */
	res = vle_opts_set("cpoptions=abc", OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_string_equal("abc", cpoptions);

	/* Restore default value. */
	res = vle_opts_set("cpoptions=", OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_string_equal("", cpoptions);

	/* Try resetting the value. */
	cpoptions_handler_calls = 0;
	res = vle_opts_set("cpoptions&", OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_string_equal("", cpoptions);
	assert_int_equal(0, cpoptions_handler_calls);
}

TEST(set_option_for_charset)
{
	optval_t val;

	cpoptions_handler_calls = 0;

	val.str_val = "abc";
	vle_opts_assign("cpoptions", val, OPT_GLOBAL);
	assert_int_equal(0, cpoptions_handler_calls);
}

TEST(charset_is_not_changed_initially)
{
	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("", OPT_GLOBAL));
	assert_string_equal("", vle_tb_get_data(vle_err));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
