#include <stic.h>

#include <stddef.h> /* wchar_t */
#include <stdlib.h> /* free() */

#include "../../src/utils/str.h"

TEST(to_wide_force_does_not_fail_on_broken_string)
{
	wchar_t *const wide = to_wide_force("01 R\366yksopp - You Know I Have To Go "
			"(\326zg\374r \326zkan 5 AM Edit).mp3");
	assert_non_null(wide);
	free(wide);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
