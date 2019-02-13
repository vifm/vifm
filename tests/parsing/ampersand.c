#include <stic.h>

#include "../../src/engine/options.h"
#include "../../src/engine/parsing.h"
#include "../../src/utils/macros.h"

#include "asserts.h"

static void dummy_handler(OPT_OP op, optval_t val);

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

SETUP()
{
	static int option_changed;
	optval_t val;

	vle_opts_init(&option_changed, NULL);

	val.str_val = "bc";
	vle_opts_add("cpoptions", "cpo", "descr", OPT_CHARSET, OPT_GLOBAL,
			ARRAY_LEN(cpoptions_vals), cpoptions_vals, dummy_handler, val);

	val.bool_val = 0;
	vle_opts_add("fastrun", "fr", "descr", OPT_BOOL, OPT_GLOBAL, 0, NULL,
			dummy_handler, val);

	val.str_val = "fusehome-default";
	vle_opts_add("fusehome", "fh", "descr", OPT_STR, OPT_GLOBAL, 0, NULL,
			dummy_handler, val);

	val.str_val = "%r/.vifm-Trash,$HOME/.vifm/Trash";
	vle_opts_add("trashdir", "td", "descr", OPT_STRLIST, OPT_GLOBAL, 0, NULL,
			dummy_handler, val);

	val.enum_item = 1;
	vle_opts_add("sort", "so", "descr", OPT_ENUM, OPT_GLOBAL,
			ARRAY_LEN(sort_enum), sort_enum, &dummy_handler, val);

	/* Parsing unit doesn't accept options with underscores, although options unit
	 * doesn't reject them.  Should it? */
	val.enum_item = 1;
	vle_opts_add("so_rt", "", "descr", OPT_ENUM, OPT_GLOBAL, ARRAY_LEN(sort_enum),
			sort_enum, &dummy_handler, val);

	val.int_val = 2;
	vle_opts_add("tabstop", "ts", "descr", OPT_INT, OPT_GLOBAL, 0, NULL,
			&dummy_handler, val);

	val.int_val = 8;
	vle_opts_add("tabstop", "ts", "descr", OPT_INT, OPT_LOCAL, 0, NULL,
			&dummy_handler, val);

	val.set_items = 0x81;
	vle_opts_add("vifminfo", "", "descr", OPT_SET, OPT_GLOBAL,
			ARRAY_LEN(vifminfo_set), vifminfo_set, &dummy_handler, val);
}

TEARDOWN()
{
	vle_opts_reset();
}

static void
dummy_handler(OPT_OP op, optval_t val)
{
}

TEST(nothing_follows_fail)
{
	ASSERT_FAIL("&", PE_INVALID_EXPRESSION);
}

TEST(space_follows_fail)
{
	ASSERT_FAIL("& ", PE_INVALID_EXPRESSION);
}

TEST(number_follows_fail)
{
	ASSERT_FAIL("&1", PE_INVALID_EXPRESSION);
}

TEST(wrong_option_name_fail)
{
	ASSERT_FAIL("&nosuchoption", PE_INVALID_EXPRESSION);
}

TEST(correct_full_option_name_ok)
{
	ASSERT_INT_OK("&tabstop", 8);
}

TEST(correct_local_option_name_ok)
{
	ASSERT_INT_OK("&l:tabstop", 8);
}

TEST(correct_global_option_name_ok)
{
	ASSERT_INT_OK("&g:tabstop", 2);
}

TEST(wrong_option_syntax_fail)
{
	ASSERT_FAIL("&x:tabstop", PE_INVALID_EXPRESSION);
}

TEST(correct_short_option_name_ok)
{
	ASSERT_INT_OK("&ts", 8);
}

TEST(concatenation_ok)
{
	ASSERT_OK("&ts.&ts", "88");
}

TEST(all_fail)
{
	ASSERT_FAIL("&all", PE_INVALID_EXPRESSION);
}

TEST(bool_option_ok)
{
	ASSERT_INT_OK("&fastrun", 0);
}

TEST(int_option_ok)
{
	ASSERT_INT_OK("&tabstop", 8);
}

TEST(str_option_ok)
{
	ASSERT_OK("&fusehome", "fusehome-default");
}

TEST(strlist_option_ok)
{
	ASSERT_OK("&trashdir", "%r/.vifm-Trash,$HOME/.vifm/Trash");
}

TEST(enum_option_ok)
{
	ASSERT_OK("&sort", "name");
}

TEST(set_option_ok)
{
	ASSERT_OK("&vifminfo", "options,cs");
}

TEST(charset_option_ok)
{
	ASSERT_OK("&cpoptions", "bc");
}

TEST(wrong_char_is_option_name_fail)
{
	ASSERT_FAIL("&so_rt", PE_INVALID_EXPRESSION);
}

TEST(very_long_option_name_fail)
{
	ASSERT_FAIL("&ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
			PE_INVALID_EXPRESSION);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
