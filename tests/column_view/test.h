#ifndef VIFM_TESTS__COLUMN_VIEW__TEST_H__
#define VIFM_TESTS__COLUMN_VIEW__TEST_H__

#include "../../src/ui/column_view.h"

#define COL1_ID 1
#define COL2_ID 2

/* This is version of column_line_print_func with fewer number of parameters.
 * Omitted those which are provide additional information and are not very
 * valuable for tests. */
typedef void (*print_func)(const char buf[], int offset, AlignType align,
		const format_info_t *info);

print_func print_next;
column_line_match_func match_next;

column_func col1_next;
column_func col2_next;

#endif /* VIFM_TESTS__COLUMN_VIEW__TEST_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
