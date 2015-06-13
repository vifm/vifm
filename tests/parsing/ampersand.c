#include <stic.h>

#include "../../src/engine/options.h"
#include "../../src/engine/parsing.h"
#include "../../src/utils/macros.h"

#include "asserts.h"

static void dummy_handler(OPT_OP op, optval_t val);

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

SETUP()
{
	static int option_changed;
	optval_t val;

	init_options(&option_changed);

	val.str_val = "bc";
	add_option("cpoptions", "cpo", OPT_CHARSET, ARRAY_LEN(cpoptions_charset) - 1,
			&cpoptions_vals, dummy_handler, val);

	val.bool_val = 0;
	add_option("fastrun", "fr", OPT_BOOL, 0, NULL, dummy_handler, val);

	val.str_val = "fusehome-default";
	add_option("fusehome", "fh", OPT_STR, 0, NULL, dummy_handler, val);

	val.str_val = "%r/.vifm-Trash,$HOME/.vifm/Trash";
	add_option("trashdir", "fh", OPT_STRLIST, 0, NULL, dummy_handler, val);

	val.enum_item = 1;
	add_option("sort", "so", OPT_ENUM, ARRAY_LEN(sort_enum), sort_enum,
			&dummy_handler, val);

	val.int_val = 8;
	add_option("tabstop", "ts", OPT_INT, 0, NULL, &dummy_handler, val);

	val.set_items = 0x81;
	add_option("vifminfo", "", OPT_SET, ARRAY_LEN(vifminfo_set), vifminfo_set,
			&dummy_handler, val);
}

TEARDOWN()
{
	clear_options();
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

TEST(very_long_option_name_fail)
{
	ASSERT_FAIL("&ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
			PE_INVALID_EXPRESSION);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
