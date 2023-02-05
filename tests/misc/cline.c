#include <stic.h>

#include <string.h> /* strdup() */

#include "../../src/ui/colored_line.h"

TEST(left_ellipsis_for_cline)
{
	cline_t cline = {
		.line = strdup("12345"),
		.line_len = strlen(cline.line),
		.attrs = strdup(""),
		.attrs_len = strlen(cline.attrs),
	};

	cline_left_ellipsis(&cline, 2, "abc");
	assert_string_equal("ab", cline.line);
	assert_int_equal(strlen(cline.line), cline.line_len);
	assert_string_equal("  ", cline.attrs);
	assert_int_equal(strlen(cline.attrs), cline.attrs_len);

	cline_dispose(&cline);
}

TEST(append_works)
{
	cline_t left = {
		.line = strdup("left"),
		.line_len = strlen(left.line),
		.attrs = strdup(""),
		.attrs_len = strlen(left.attrs),
	};

	cline_t right = {
		.line = strdup("right"),
		.line_len = strlen(right.line),
		.attrs = strdup(""),
		.attrs_len = strlen(right.attrs),
	};

	cline_append(&left, &right);
	assert_string_equal("leftright", left.line);
	assert_int_equal(strlen(left.line), left.line_len);
	assert_string_equal("    ", left.attrs);
	assert_int_equal(strlen(left.attrs), left.attrs_len);

	cline_dispose(&left);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
