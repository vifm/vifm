#include <stic.h>

#include <string.h>

#include "../../src/engine/options.h"
#include "../../src/utils/macros.h"

static void cpoptions_handler(OPT_OP op, optval_t val);
static void fastrun_handler(OPT_OP op, optval_t val);
static void fusehome_handler(OPT_OP op, optval_t val);
static void tabstop_handler(OPT_OP op, optval_t val);
static void vifminfo_handler(OPT_OP op, optval_t val);
static void dummy_handler(OPT_OP op, optval_t val);

char cpoptions[10];
int cpoptions_handler_calls;
int fastrun;
int fusehome_handler_calls;
int tabstop;
const char *value;
int vifminfo;
int vifminfo_handler_calls;

static const char cpoptions_charset[] = "abc";
static const char * cpoptions_vals = cpoptions_charset;

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

DEFINE_SUITE();

SETUP()
{
	static int option_changed;
	optval_t val;

	init_options(&option_changed);

	val.str_val = "";
	cpoptions[0] = '\0';
	add_option("cpoptions", "cpo", OPT_CHARSET, OPT_GLOBAL,
			ARRAY_LEN(cpoptions_charset) - 1, &cpoptions_vals, cpoptions_handler,
			val);

	val.str_val = "";
	add_option("cdpath", "cd", OPT_STRLIST, OPT_GLOBAL, 0, NULL, dummy_handler,
			val);

	val.bool_val = fastrun = 0;
	add_option("fastrun", "fr", OPT_BOOL, OPT_GLOBAL, 0, NULL, fastrun_handler,
			val);

	value = val.str_val = "fusehome-default";
	add_option("fusehome", "fh", OPT_STR, OPT_GLOBAL, 0, NULL, fusehome_handler,
			val);

	val.enum_item = 1;
	add_option("sort", "so", OPT_ENUM, OPT_GLOBAL, ARRAY_LEN(sort_enum),
			sort_enum, &dummy_handler, val);

	val.bool_val = 1;
	add_option("sortorder", "", OPT_BOOL, OPT_GLOBAL, 0, NULL, &dummy_handler,
			val);

	val.int_val = tabstop = 8;
	add_option("tabstop", "ts", OPT_INT, OPT_GLOBAL, 0, NULL, &tabstop_handler,
			val);

	val.set_items = vifminfo = 0;
	add_option("vifminfo", "", OPT_SET, OPT_GLOBAL, ARRAY_LEN(vifminfo_set),
			vifminfo_set, &vifminfo_handler, val);
}

TEARDOWN()
{
	clear_options();
}

static void
cpoptions_handler(OPT_OP op, optval_t val)
{
	strcpy(cpoptions, val.str_val);
	cpoptions_handler_calls++;
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
	fusehome_handler_calls++;
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
