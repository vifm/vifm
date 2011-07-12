#include "seatest.h"

#include "../../src/macros.h"
#include "../../src/options.h"

const char *value;

static const char * sort_enum[] = {
	"ext",
	"name",
	"gid",
	"gname",
	"mode",
	"uid",
	"uname",
	"size",
	"atime",
	"ctime",
	"mtime",
};

void test_quotes(void);
void opt_completion(void);

void all_tests(void)
{
	test_quotes();
	opt_completion();
}

static void
fusehome_handler(enum opt_op op, union optval_t val)
{
	value = val.str_val;
}

static void
sort_handler(enum opt_op op, union optval_t val)
{
}

static void
setup(void)
{
	static int option_changed;

	init_options(&option_changed, NULL);

	add_option("fusehome", OPT_STR, 0, NULL, fusehome_handler);
	add_option("sort", OPT_ENUM, ARRAY_LEN(sort_enum), sort_enum, &sort_handler);
}

int main(int argc, char **argv)
{
	suite_setup(setup);
	suite_teardown(clear_options);

	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
