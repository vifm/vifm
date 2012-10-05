#include "seatest.h"

#include "../../src/engine/options.h"
#include "../../src/utils/macros.h"

int fastrun;
int tabstop;
const char *value;
int vifminfo;
int vifminfo_handler_calls;

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
void input_tests(void);
void set_tests(void);

void
all_tests(void)
{
	test_quotes();
	opt_completion();
	with_spaces_tests();
	input_tests();
	set_tests();
}

static void
fastrun_handler(OPT_OP op, optval_t val)
{
	fastrun = val.bool_val;
}

static void
fusehome_handler(OPT_OP op, optval_t val)
{
	value = val.str_val;
}

static void
tabstop_handler(OPT_OP op, optval_t val)
{
	tabstop = val.int_val;
}

static void
vifminfo_handler(OPT_OP op, optval_t val)
{
	vifminfo = val.set_items;
	vifminfo_handler_calls++;
}

static void
dummy_handler(OPT_OP op, optval_t val)
{
}

static void
setup(void)
{
	static int option_changed;
	optval_t val;

	init_options(&option_changed);

	val.bool_val = 0;
	add_option("fastrun", "fr", OPT_BOOL, 0, NULL, fastrun_handler, val);
	val.str_val = "";
	add_option("fusehome", "fh", OPT_STR, 0, NULL, fusehome_handler, val);
	val.enum_item = 1;
	add_option("sort", "so", OPT_ENUM, ARRAY_LEN(sort_enum), sort_enum,
			&dummy_handler, val);
	add_option("sortorder", "", OPT_BOOL, 0, NULL, &dummy_handler, val);
	val.int_val = 8;
	add_option("tabstop", "ts", OPT_INT, 0, NULL, &tabstop_handler, val);
	val.set_items = 0;
	add_option("vifminfo", "", OPT_SET, ARRAY_LEN(vifminfo_set), vifminfo_set,
			&vifminfo_handler, val);
}

int
main(int argc, char *argv[])
{
	suite_setup(setup);
	suite_teardown(clear_options);

	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
