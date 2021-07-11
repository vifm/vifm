#include <stic.h>

#include "../../src/compat/reallocarray.h"
#include "../../src/utils/string_array.h"
#include "../../src/version.h"

TEST(stats_are_included)
{
	int max_len = fill_version_info(NULL, /*include_stats=*/0);
	char **list = reallocarray(NULL, max_len, sizeof(*list));
	int no_stats_len = fill_version_info(list, /*include_stats=*/0);
	free_string_array(list, no_stats_len);

	max_len = fill_version_info(NULL, /*include_stats=*/1);
	list = reallocarray(NULL, max_len, sizeof(*list));
	int stats_len = fill_version_info(list, /*include_stats=*/1);
	free_string_array(list, stats_len);

	assert_true(stats_len > no_stats_len);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
