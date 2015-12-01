#include <stic.h>

#include <stddef.h> /* NULL */

#include "../../src/engine/autocmds.h"

static void dummy_handler(const char action[], void *arg);
static void list_handler(const char event[], const char pattern[],
		const char action[], void *arg);

static int count;

TEARDOWN()
{
	count = 0;
}

TEST(callback_not_invoked_for_no_entries)
{
	vle_aucmd_list(NULL, NULL, &list_handler, NULL);
	assert_int_equal(0, count);
}

TEST(particular_event_is_listed)
{
	assert_success(vle_aucmd_on_execute("cd", "/path", "action", &dummy_handler));
	assert_success(vle_aucmd_on_execute("cd", "/pat", "action", &dummy_handler));
	assert_success(vle_aucmd_on_execute("go", "/pat", "action", &dummy_handler));

	vle_aucmd_list("cd", NULL, &list_handler, NULL);
	assert_int_equal(2, count);
}

TEST(particular_pattern_is_listed)
{
	assert_success(vle_aucmd_on_execute("cd", "/path", "action", &dummy_handler));
	assert_success(vle_aucmd_on_execute("cd", "/path", "action", &dummy_handler));
	assert_success(vle_aucmd_on_execute("cd", "/pat", "action", &dummy_handler));
	assert_success(vle_aucmd_on_execute("go", "/pat", "action", &dummy_handler));

	vle_aucmd_list("cd", "/path", &list_handler, NULL);
	assert_int_equal(2, count);
}

static void
dummy_handler(const char a[], void *arg)
{
}

static void
list_handler(const char event[], const char pattern[], const char action[],
		void *arg)
{
	++count;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
