#include "seatest.h"

#include <stdlib.h> /* free() */
#include <string.h> /* strchr() strcmp() */

#include "../../src/cfg/config.h"
#include "../../src/ui/statusline.h"

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

static void
setup(void)
{
	cfg.time_format = strdup("");

	lwin.list_rows = 1;
	lwin.list_pos = 0;
	lwin.dir_entry = calloc(lwin.list_rows, sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("file");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];

	curr_view = &lwin;
	other_view = &rwin;
}

static void
teardown(void)
{
	int i;

	for(i = 0; i < lwin.list_rows; ++i)
	{
		free(lwin.dir_entry[i].name);
	}
	free(lwin.dir_entry);

	free(cfg.time_format);
}

static void
test_empty_format(void)
{
	const char *const format = "";
	char *const expanded = expand_status_line_macros(&lwin, format);
	assert_string_equal(format, expanded);
	free(expanded);
}

static void
test_no_macros(void)
{
	const char *const format = "No formatting here";
	char *const expanded = expand_status_line_macros(&lwin, format);
	assert_string_equal(format, expanded);
	free(expanded);
}

static void
test_t_macro_expanded(void)
{
	ASSERT_EXPANDED("%t");
}

static void
test_A_macro_expanded(void)
{
	ASSERT_EXPANDED("%A");
}

static void
test_u_macro_expanded(void)
{
	ASSERT_EXPANDED("%u");
}

static void
test_g_macro_expanded(void)
{
	ASSERT_EXPANDED("%g");
}

static void
test_s_macro_expanded(void)
{
	ASSERT_EXPANDED("%s");
}

static void
test_E_macro_expanded(void)
{
	ASSERT_EXPANDED("%E");
}

static void
test_d_macro_expanded(void)
{
	ASSERT_EXPANDED("%d");
}

static void
test_l_macro_expanded(void)
{
	ASSERT_EXPANDED("%l");
}

static void
test_L_macro_expanded(void)
{
	ASSERT_EXPANDED("%L");
}

static void
test_dash_macro_expanded(void)
{
	ASSERT_EXPANDED("%-t");
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

static void
test_S_macro_expanded(void)
{
	ASSERT_EXPANDED("%S");
}

static void
test_percent_macro_expanded(void)
{
	ASSERT_EXPANDED_TO("%%", "%");
}

static void
test_wrong_macros_ignored(void)
{
	static const char STATUS_CHARS[] = "tAugsEd-lLS%0123456789";
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

static void
test_wrong_macros_with_width_field_ignored(void)
{
	static const char STATUS_CHARS[] = "tAugsEd-lLS%0123456789";
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

void
expand_status_line_macros_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_empty_format);
	run_test(test_no_macros);

	run_test(test_t_macro_expanded);
	run_test(test_A_macro_expanded);
	run_test(test_u_macro_expanded);
	run_test(test_g_macro_expanded);
	run_test(test_s_macro_expanded);
	run_test(test_E_macro_expanded);
	run_test(test_d_macro_expanded);
	run_test(test_l_macro_expanded);
	run_test(test_L_macro_expanded);
	run_test(test_dash_macro_expanded);
	run_test(test_S_macro_expanded);
	run_test(test_percent_macro_expanded);

	run_test(test_wrong_macros_ignored);
	run_test(test_wrong_macros_with_width_field_ignored);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
