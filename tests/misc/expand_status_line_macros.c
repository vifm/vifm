#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h> /* strchr() strcmp() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/parsing.h"
#include "../../src/ui/statusline.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/env.h"
#include "../../src/utils/str.h"
#include "../../src/status.h"

/* Checks that expanded string isn't equal to format string. */
#define ASSERT_EXPANDED(format) \
	do \
	{ \
		cline_t result = expand_status_line_macros(&lwin, format); \
		free(result.attrs); \
		char *const expanded = result.line; \
		assert_false(strcmp(expanded, format) == 0); \
		free(expanded); \
	} \
	while(0)

/* Checks that expanded string is equal to expected string. */
#define ASSERT_EXPANDED_TO(format, expected) \
	do \
	{ \
		cline_t result = expand_status_line_macros(&lwin, format); \
		free(result.attrs); \
		char *const expanded = result.line; \
		assert_string_equal(expected, expanded); \
		free(expanded); \
	} \
	while(0)

/* Checks that expanded string is equal to expected string and has expected
 * highlighting. */
#define ASSERT_EXPANDED_TO_WITH_HI(format, expected, highlighting) \
	do \
	{ \
		cline_t result = expand_status_line_macros(&lwin, format); \
		char *const expanded = result.line; \
		assert_string_equal(expected, expanded); \
		assert_string_equal(highlighting, result.attrs); \
		free(expanded); \
		free(result.attrs); \
	} \
	while(0)

SETUP_ONCE()
{
	init_parser(&env_get);
	try_enable_utf8_locale();
}

SETUP()
{
	update_string(&cfg.time_format, "+");

	lwin.list_rows = 1;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("file");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];

	curr_view = &lwin;
	other_view = &rwin;

	cfg.sizefmt.ieci_prefixes = 0;
	cfg.sizefmt.base = 1024;
	cfg.sizefmt.precision = 0;
	cfg.sizefmt.space = 1;
}

TEARDOWN()
{
	int i;

	for(i = 0; i < lwin.list_rows; ++i)
	{
		free(lwin.dir_entry[i].name);
	}
	dynarray_free(lwin.dir_entry);

	update_string(&cfg.time_format, NULL);
}

TEST(empty_format)
{
	ASSERT_EXPANDED_TO("", "");
}

TEST(no_macros)
{
	ASSERT_EXPANDED_TO("No formatting here", "No formatting here");
}

TEST(t_macro_expanded)
{
	ASSERT_EXPANDED("%t");
}

TEST(T_macro_expanded)
{
	lwin.dir_entry[0].type = FT_LINK;
	ASSERT_EXPANDED("%T");
}

TEST(f_macro_expanded)
{
	ASSERT_EXPANDED("%f");
}

TEST(a_macro_expanded)
{
	ASSERT_EXPANDED("%a");
	strcpy(lwin.curr_dir, SANDBOX_PATH);
	ASSERT_EXPANDED("%a");
}

TEST(c_macro_expanded)
{
	ASSERT_EXPANDED("%c");
	strcpy(lwin.curr_dir, SANDBOX_PATH);
	ASSERT_EXPANDED("%c");
}

TEST(A_macro_expanded)
{
	ASSERT_EXPANDED("%A");
}

TEST(o_macro_expanded)
{
	ASSERT_EXPANDED("%o");
}

TEST(u_macro_expanded)
{
	ASSERT_EXPANDED("%u");
}

TEST(g_macro_expanded)
{
	ASSERT_EXPANDED("%g");
}

TEST(s_macro_expanded)
{
	ASSERT_EXPANDED("%s");
}

TEST(E_macro_expanded)
{
	ASSERT_EXPANDED("%E");
}

TEST(d_macro_expanded)
{
	ASSERT_EXPANDED("%d");
}

TEST(D_macro_expanded)
{
	curr_stats.number_of_windows = 1;
	ASSERT_EXPANDED("%D");
	curr_stats.number_of_windows = 2;
	ASSERT_EXPANDED("%D");
}

TEST(l_macro_expanded)
{
	ASSERT_EXPANDED("%l");
}

TEST(L_macro_expanded)
{
	ASSERT_EXPANDED("%L");
}

TEST(z_macro_expanded)
{
	ASSERT_EXPANDED("%z");
}

TEST(x_macro_expanded)
{
	ASSERT_EXPANDED("%x");
}

TEST(dash_macro_expanded)
{
	ASSERT_EXPANDED("%-t");
	ASSERT_EXPANDED("%-f");
	ASSERT_EXPANDED("%-A");
	ASSERT_EXPANDED("%-u");
	ASSERT_EXPANDED("%-g");
	ASSERT_EXPANDED("%-s");
	ASSERT_EXPANDED("%-E");
	ASSERT_EXPANDED("%-d");
	ASSERT_EXPANDED("%-l");
	ASSERT_EXPANDED("%-L");
	ASSERT_EXPANDED("%-S");
	ASSERT_EXPANDED("%-%");
}

TEST(P_macro_expanded)
{
	ASSERT_EXPANDED("%P");
}

TEST(S_macro_expanded)
{
	ASSERT_EXPANDED("%S");
}

TEST(percent_macro_expanded)
{
	ASSERT_EXPANDED_TO("%%", "%");
}

TEST(wrong_macros_ignored)
{
	static const char STATUS_CHARS[] = "tTfacAugsEdD-xlLoPS%[]z{*";
	int i;

	for(i = 1; i <= 255; ++i)
	{
		if(strchr(STATUS_CHARS, i) == NULL)
		{
			const char format[] = { '%', i, '\0' };
			ASSERT_EXPANDED_TO(format, format);
		}
	}
}

TEST(wrong_macros_with_width_field_ignored)
{
	static const char STATUS_CHARS[] = "tTfacAugsEdD-xlLoPS%[]z{*";
	int i;

	for(i = 1; i <= 255; ++i)
	{
		if(strchr(STATUS_CHARS, i) == NULL)
		{
			const char format[] = { '%', '5', i, '\0' };
			ASSERT_EXPANDED_TO(format, format);
		}
	}
}

TEST(optional_empty)
{
	lwin.filtered = 0;
	ASSERT_EXPANDED_TO("%[%0-%]", "");
}

TEST(optional_non_empty)
{
	lwin.filtered = 1;
	ASSERT_EXPANDED_TO("%[%0-%]", "1");
}

TEST(nested_optional_empty)
{
	lwin.filtered = 0;
	ASSERT_EXPANDED_TO("%[%[%0-%]%]", "");
}

TEST(nested_optional_non_empty)
{
	lwin.filtered = 1;
	ASSERT_EXPANDED_TO("%[%[%0-%]%]", "1");
}

TEST(ignore_mismatched_opening_bracket)
{
	ASSERT_EXPANDED_TO("%[", "%[");
	ASSERT_EXPANDED_TO("%[abcdef", "%[abcdef");
}

TEST(ignore_mismatched_closing_bracket)
{
	ASSERT_EXPANDED_TO("%]", "%]");
}

TEST(valid_expr)
{
	ASSERT_EXPANDED_TO("%{'a'.'b'}", "ab");
}

TEST(invalid_expr)
{
	ASSERT_EXPANDED_TO("%{foobar}", "<Invalid expr>");
}

TEST(ignore_mismatched_opening_curly_bracket)
{
	ASSERT_EXPANDED_TO("<%{>", "<%{>");
	ASSERT_EXPANDED_TO("<%{abcdef>", "<%{abcdef>");
}

TEST(highlighting_is_set_correctly)
{
	ASSERT_EXPANDED_TO_WITH_HI("[%1*%t%*]",
	                           "[file]",
	                           " 1   0");
}

TEST(highlighting_is_set_correctly_for_optional)
{
	ASSERT_EXPANDED_TO_WITH_HI("%[%1*a",
	                           "%[a",
	                           "  1");
}

TEST(highlighting_has_equals_macro_preserved)
{
	ASSERT_EXPANDED_TO_WITH_HI("%9*%t %= %t%7*",
	                           "file %= file",
	                           "9    =      ");
	ASSERT_EXPANDED_TO_WITH_HI("%9*%t%=%t%7*",
	                           "file%=file",
	                           "9   =     ");
	ASSERT_EXPANDED_TO_WITH_HI("%t%9*%=%7*%t",
	                           "file%=file",
	                           "    9=7   ");
	ASSERT_EXPANDED_TO_WITH_HI("%t%9*%=%7*%t%0*",
	                           "file%=file",
	                           "    9=7   ");
	ASSERT_EXPANDED_TO_WITH_HI("%=%1*%t%0*",
	                           "%=file",
	                           "= 1   ");
	ASSERT_EXPANDED_TO_WITH_HI("%t%1*%=%2*%t%0*",
	                           "file%=file",
	                           "    1=2   ");
}

TEST(bad_user_group_remains_in_line)
{
	ASSERT_EXPANDED_TO_WITH_HI("%10*", "%10*", "    ");
}

TEST(empty_optional_drops_attrs)
{
	ASSERT_EXPANDED_TO_WITH_HI("%1*%[%2*%]%3*",
	                           "",
	                           "");
}

TEST(non_empty_optional_preserves_attrs)
{
	ASSERT_EXPANDED_TO_WITH_HI("%1*%[%t%2*%t%]%3*",
	                           "filefile",
	                           "1   2   ");
	ASSERT_EXPANDED_TO_WITH_HI("%1*%[%2*%t%3*%t%]%4*",
	                           "filefile",
	                           "2   3   ");
	ASSERT_EXPANDED_TO_WITH_HI("%1*%[%2*%t%3*%]%4*",
	                           "file",
	                           "2   ");
}

TEST(wide_characters_do_not_break_highlighting, IF(utf8_locale))
{
	ASSERT_EXPANDED_TO_WITH_HI("%1*螺丝 %= 螺%2*丝",
	                           "螺丝 %= 螺丝",
	                           "1    =    2 ");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
