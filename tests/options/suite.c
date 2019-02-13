#include <stic.h>

#include <string.h>

#include "../../src/engine/options.h"
#include "../../src/utils/macros.h"

static void cpoptions_handler(OPT_OP op, optval_t val);
static void cdpath_handler(OPT_OP op, optval_t val);
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

static const char *cpoptions_vals[][2] = {
	{ "abc", "descr" },
	{ "a", "descr" },
	{ "b", "descr" },
	{ "c", "descr" },
};

static const char *sort_enum[][2] = {
	{ "ext",   "descr" },
	{ "name",  "descr" },
	{ "gid",   "descr" },
	{ "gname", "descr" },
	{ "mode",  "descr" },
	{ "uid",   "descr" },
	{ "uname", "descr" },
	{ "size",  "descr" },
	{ "atime", "descr" },
	{ "ctime", "descr" },
	{ "mtime", "descr" },
};

static const char *vifminfo_set[][2] = {
	{ "options",   "descr" },
	{ "filetypes", "descr" },
	{ "commands",  "descr" },
	{ "bookmarks", "descr" },
	{ "tui",       "descr" },
	{ "dhistory",  "descr" },
	{ "state",     "descr" },
	{ "cs",        "descr" },
};

DEFINE_SUITE();

SETUP()
{
	static int option_changed;
	optval_t val;

	vle_opts_init(&option_changed, NULL);

	val.str_val = "";
	cpoptions[0] = '\0';
	vle_opts_add("cpoptions", "cpo", "descr", OPT_CHARSET, OPT_GLOBAL,
			ARRAY_LEN(cpoptions_vals), cpoptions_vals, cpoptions_handler, val);

	val.str_val = "";
	vle_opts_add("cdpath", "cd", "descr", OPT_STRLIST, OPT_GLOBAL, 0, NULL,
			cdpath_handler, val);

	val.bool_val = fastrun = 0;
	vle_opts_add("fastrun", "fr", "descr", OPT_BOOL, OPT_GLOBAL, 0, NULL,
			fastrun_handler, val);

	value = val.str_val = "fusehome-default";
	vle_opts_add("fusehome", "fh", "descr", OPT_STR, OPT_GLOBAL, 0, NULL,
			fusehome_handler, val);

	val.enum_item = 1;
	vle_opts_add("sort", "so", "descr", OPT_ENUM, OPT_GLOBAL,
			ARRAY_LEN(sort_enum), sort_enum, &dummy_handler, val);

	val.bool_val = 1;
	vle_opts_add("sortorder", "", "descr", OPT_BOOL, OPT_GLOBAL, 0, NULL,
			&dummy_handler, val);

	val.int_val = tabstop = 8;
	vle_opts_add("tabstop", "ts", "descr", OPT_INT, OPT_GLOBAL, 0, NULL,
			&tabstop_handler, val);

	val.set_items = vifminfo = 0;
	vle_opts_add("vifminfo", "", "descr", OPT_SET, OPT_GLOBAL,
			ARRAY_LEN(vifminfo_set), vifminfo_set, &vifminfo_handler, val);
}

TEARDOWN()
{
	vle_opts_reset();
}

static void
cpoptions_handler(OPT_OP op, optval_t val)
{
	strcpy(cpoptions, val.str_val);
	cpoptions_handler_calls++;
}

static void
cdpath_handler(OPT_OP op, optval_t val)
{
	value = val.str_val;
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
