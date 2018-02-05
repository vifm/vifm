#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h> /* strchr() strcmp() */

#include "../../src/cfg/config.h"
#include "../../src/ui/statusline.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/str.h"
#include "../../src/status.h"

/* Checks that expanded string isn't equal to format string. */
#define ASSERT_EXPANDED(format) \
	do \
	{ \
		char *const expanded = expand_status_line_macros(&lwin, format); \
		assert_false(strcmp(expanded, format) == 0); \
		free(expanded); \
	} \
	while(0)

/* Checks that expanded string is equal to expected string. */
#define ASSERT_EXPANDED_TO(format, expected) \
	do \
	{ \
		char *const expanded = expand_status_line_macros(&lwin, format); \
		assert_string_equal(expected, expanded); \
		free(expanded); \
	} \
	while(0)

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
	const char *const format = "";
	char *const expanded = expand_status_line_macros(&lwin, format);
	assert_string_equal(format, expanded);
	free(expanded);
}

TEST(no_macros)
{
	const char *const format = "No formatting here";
	char *const expanded = expand_status_line_macros(&lwin, format);
	assert_string_equal(format, expanded);
	free(expanded);
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

TEST(A_macro_expanded)
{
	ASSERT_EXPANDED("%A");
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
	static const char STATUS_CHARS[] = "tTfaAugsEdD-xlLS%[]z{";
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
	static const char STATUS_CHARS[] = "tTfaAugsEdD-xlLS%[]z{";
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
	char *v = getenv("HOME");
	if(v == NULL)
	{
		v = "";
	}
	ASSERT_EXPANDED_TO("%{$HOME}", v);
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
