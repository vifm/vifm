#include "seatest.h"

#include "../../src/macros.h"
#include "../../src/options.h"

int fastrun;
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

static const char * vifminfo_set[] = {
	"options",
	"filetypes",
	"commands",
	"bookmarks",
	"tui",
	"dhistory",
	"state",
	"cs",
};

void test_quotes(void);
void opt_completion(void);
void with_spaces_tests(void);

void all_tests(void)
{
	test_quotes();
	opt_completion();
	with_spaces_tests();
}

static void
fastrun_handler(enum opt_op op, union optval_t val)
{
	fastrun = val.bool_val;
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
vifminfo_handler(enum opt_op op, union optval_t val)
{
}

static void
setup(void)
{
	static int option_changed;

	init_options(&option_changed, NULL);

	add_option("fastrun", "fr", OPT_BOOL, 0, NULL, fastrun_handler);
	add_option("fusehome", "fh", OPT_STR, 0, NULL, fusehome_handler);
	add_option("sort", "so", OPT_ENUM, ARRAY_LEN(sort_enum), sort_enum,
			&sort_handler);
	add_option("vifminfo", "", OPT_SET, ARRAY_LEN(vifminfo_set), vifminfo_set,
			&vifminfo_handler);
}

int main(int argc, char **argv)
{
	suite_setup(setup);
	suite_teardown(clear_options);

	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
