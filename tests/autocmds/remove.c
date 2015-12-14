#include <stic.h>

#include <stddef.h> /* NULL */

#include "../../src/engine/autocmds.h"

static void handler(const char action[], void *arg);

static const char *action;

TEARDOWN()
{
	action = NULL;
}

TEST(negation_is_considered_on_removal)
{
	assert_success(vle_aucmd_on_execute("cd", "~", "~", &handler));
	assert_success(vle_aucmd_on_execute("cd", "!~", "!~", &handler));

	vle_aucmd_remove("cd", "!~");

	vle_aucmd_execute("cd", "~", NULL);
	assert_string_equal("~", action);
	action = NULL;
	vle_aucmd_execute("cd", "~/tmp", NULL);
	assert_string_equal(NULL, action);
}

static void
handler(const char a[], void *arg)
{
	action = a;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
