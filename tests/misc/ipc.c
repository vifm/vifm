#include <stic.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../../src/utils/str.h"
#include "../../src/utils/string_array.h"
#include "../../src/ipc.h"

static void test_ipc_args(char *args[]);
static void test_ipc_args2(char *args[]);
static int enabled_and_not_in_wine(void);

static const char NAME[] = "vifm-test";
static int nmessages;
static char *message;
static int nmessages2;
static char *message2;

TEARDOWN()
{
	nmessages = 0;
	nmessages2 = 0;
	update_string(&message, NULL);
	update_string(&message2, NULL);
}

TEST(destroy_null, IF(ipc_enabled))
{
	ipc_free(NULL);
}

TEST(create_and_destroy, IF(ipc_enabled))
{
	ipc_t *const ipc = ipc_init(NAME, &test_ipc_args);
	assert_non_null(ipc);
	ipc_free(ipc);
}

TEST(name_can_be_null, IF(ipc_enabled))
{
	ipc_t *const ipc = ipc_init(NULL, &test_ipc_args);
	assert_non_null(ipc);
	ipc_free(ipc);
}

TEST(name_is_taken_into_account, IF(ipc_enabled))
{
	ipc_t *const ipc = ipc_init(NAME, &test_ipc_args);
	assert_true(starts_with_lit(ipc_get_name(ipc), NAME));
	ipc_free(ipc);
}

TEST(instance_is_listed_when_it_exists, IF(enabled_and_not_in_wine))
{
	int len;
	ipc_t *const ipc = ipc_init(NAME, &test_ipc_args);
	char **const list = ipc_list(&len);
	assert_true(is_in_string_array(list, len, ipc_get_name(ipc)));
	free_string_array(list, len);
	ipc_free(ipc);
}

TEST(instance_is_not_listed_after_it_is_freed, IF(ipc_enabled))
{
	int len;
	char **list;

	ipc_t *const ipc = ipc_init(NAME, &test_ipc_args);
	char *const name = strdup(ipc_get_name(ipc));
	ipc_free(ipc);

	list = ipc_list(&len);
	assert_false(is_in_string_array(list, len, name));
	free_string_array(list, len);

	free(name);
}

TEST(message_is_delivered, IF(enabled_and_not_in_wine))
{
	char msg[] = "test message";
	char *data[] = { msg, NULL };

	ipc_t *const ipc1 = ipc_init(NAME, &test_ipc_args);
	ipc_t *const ipc2 = ipc_init(NAME, &test_ipc_args2);

	assert_success(ipc_send(ipc1, ipc_get_name(ipc2), data));
	ipc_check(ipc1);
	ipc_check(ipc2);
	ipc_check(ipc2);

	ipc_free(ipc1);
	ipc_free(ipc2);

	assert_int_equal(0, nmessages);
	assert_string_equal(NULL, message);
	assert_int_equal(2, nmessages2);
	assert_string_equal(msg, message2);
}

static void
test_ipc_args(char *args[])
{
	nmessages += count_strings(args);
	update_string(&message, args[1]);
}

static void
test_ipc_args2(char *args[])
{
	nmessages2 += count_strings(args);
	update_string(&message2, args[1]);
}

static int
enabled_and_not_in_wine(void)
{
#ifndef _WIN32
	return ipc_enabled();
#else
	const HMODULE dll = GetModuleHandle("ntdll.dll");
	if(dll == INVALID_HANDLE_VALUE)
	{
		return 0;
	}

	/* Apparently, WINE doesn't implement some things related to named pipes. */
	return (ipc_enabled() && GetProcAddress(dll, "wine_get_version") == NULL);
#endif
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
