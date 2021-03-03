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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
