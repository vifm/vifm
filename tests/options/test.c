#include "seatest.h"

#include "../../src/options.h"

const char *value;

void test_quotes(void);

void all_tests(void)
{
	test_quotes();
}

static void
fusehome_handler(enum opt_op op, union optval_t val)
{
	value = val.str_val;
}

static void
setup(void)
{
	static int option_changed;

	init_options(&option_changed, NULL);

	add_option("fusehome", OPT_STR, 0, NULL, fusehome_handler);
}

int main(int argc, char **argv)
{
	suite_setup(setup);
	suite_teardown(clear_options);

	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
