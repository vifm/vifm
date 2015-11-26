#include <stic.h>

#include <stddef.h> /* NULL */

#include "../../src/engine/autocmds.h"

static void handler(const char action[]);

static const char *action;

TEARDOWN()
{
	action = NULL;
}

TEST(different_path_does_not_match)
{
	assert_success(vle_aucmd_on_execute("cd", "/path", "action", &handler));
	vle_aucmd_execute("cd", "/other");
	assert_string_equal(NULL, action);
}

TEST(same_path_matches)
{
	assert_success(vle_aucmd_on_execute("cd", "/path", "action", &handler));

	vle_aucmd_execute("cd", "/path");
	assert_string_equal("action", action);
}

static void
handler(const char a[])
{
	action = a;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
