#ifndef VIFM_TESTS__FILETYPE__TEST_H__
#define VIFM_TESTS__FILETYPE__TEST_H__

#include "../../src/ui/column_view.h"

void set_programs(const char pattern[], const char programs[], int for_x,
		int in_x);

void set_viewers(const char pattern[], const char viewers[]);

int has_mime_type_detection(void);

#endif /* VIFM_TESTS__FILETYPE__TEST_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
