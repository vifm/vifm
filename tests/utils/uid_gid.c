#include <stic.h>

#include <test-utils.h>

#include "../../src/ui/ui.h"
#include "../../src/utils/utils.h"

TEST(uid_is_cached_correctly, IF(not_windows))
{
	char buf[32];
	struct dir_entry_t entry = { };

	/* This was caching result for UID which then got returned on the call with
	 * as_num set (and vice versa, but easier to check it in this order). */
	get_uid_string(&entry, /*as_num=*/0, sizeof(buf), buf);

	get_uid_string(&entry, /*as_num=*/1, sizeof(buf), buf);
	assert_string_equal("0", buf);
}

TEST(gid_is_cached_correctly, IF(not_windows))
{
	char buf[32];
	struct dir_entry_t entry = { };

	/* This was caching result for UID which then got returned on the call with
	 * as_num set (and vice versa, but easier to check it in this order). */
	get_gid_string(&entry, /*as_num=*/0, sizeof(buf), buf);

	get_gid_string(&entry, /*as_num=*/1, sizeof(buf), buf);
	assert_string_equal("0", buf);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
