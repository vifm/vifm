#include <stic.h>

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() */
#include <string.h> /* strcmp() strdup() */

#include <test-utils.h>

#include "../../src/utils/str.h"
#include "../../src/utils/string_array.h"
#include "../../src/background.h"
#include "../../src/ipc.h"

static void test_ipc_args(char *args[]);
static void test_ipc_args2(char *args[]);
static void recursive_ipc_args(char *args[]);
static char * test_ipc_eval(const char expr[]);
static char * test_ipc_eval_error(const char expr[]);
static void other_instance(bg_op_t *bg_op, void *arg);
static int enabled_and_not_in_wine(void);
static int enabled_and_not_windows(void);

static const char NAME[] = "vifm-test";
static int nmessages;
static char *message;
static int nmessages2;
static char *message2;
static ipc_t *recursive_ipc;

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
	ipc_t *const ipc = ipc_init(NAME, &test_ipc_args, &test_ipc_eval);
	assert_non_null(ipc);
	ipc_free(ipc);
}

TEST(name_can_be_null, IF(ipc_enabled))
{
	ipc_t *const ipc = ipc_init(NULL, &test_ipc_args, &test_ipc_eval);
	assert_non_null(ipc);
	ipc_free(ipc);
}

TEST(name_is_taken_into_account, IF(ipc_enabled))
{
	ipc_t *const ipc = ipc_init(NAME, &test_ipc_args, &test_ipc_eval);
	assert_true(starts_with_lit(ipc_get_name(ipc), NAME));
	ipc_free(ipc);
}

TEST(names_do_not_repeat, IF(enabled_and_not_in_wine))
{
	ipc_t *const ipc1 = ipc_init(NAME, &test_ipc_args, &test_ipc_eval);
	ipc_t *const ipc2 = ipc_init(NAME, &test_ipc_args, &test_ipc_eval);
	assert_true(strcmp(ipc_get_name(ipc1), ipc_get_name(ipc2)) != 0);
	ipc_free(ipc1);
	ipc_free(ipc2);
}

TEST(instance_is_listed_when_it_exists, IF(enabled_and_not_in_wine))
{
	int len;
	ipc_t *const ipc = ipc_init(NAME, &test_ipc_args, &test_ipc_eval);
	char **const list = ipc_list(&len);
	assert_true(is_in_string_array(list, len, ipc_get_name(ipc)));
	free_string_array(list, len);
	ipc_free(ipc);
}

TEST(instance_is_not_listed_after_it_is_freed, IF(ipc_enabled))
{
	int len;
	char **list;

	ipc_t *const ipc = ipc_init(NAME, &test_ipc_args, &test_ipc_eval);
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

	ipc_t *const ipc1 = ipc_init(NAME, &test_ipc_args, &test_ipc_eval);
	ipc_t *const ipc2 = ipc_init(NAME, &test_ipc_args2, &test_ipc_eval);

	assert_success(ipc_send(ipc1, ipc_get_name(ipc2), data));
	assert_false(ipc_check(ipc1));
	assert_true(ipc_check(ipc2));
	assert_false(ipc_check(ipc2));

	ipc_free(ipc1);
	ipc_free(ipc2);

	assert_int_equal(0, nmessages);
	assert_string_equal(NULL, message);
	assert_int_equal(2, nmessages2);
	assert_string_equal(msg, message2);
}

TEST(expr_is_evaluated, IF(enabled_and_not_in_wine))
{
	const char expr[] = "good expression";
	char *result;

	ipc_t *const ipc1 = ipc_init(NAME, &test_ipc_args, &test_ipc_eval);
	ipc_t *const ipc2 = ipc_init(NAME, &test_ipc_args2, &test_ipc_eval);

	assert_success(bg_execute("", "", 0, 1, &other_instance, ipc2));

	result = ipc_eval(ipc1, ipc_get_name(ipc2), expr);
	assert_false(ipc_check(ipc1));

	wait_for_bg();

	ipc_free(ipc1);
	ipc_free(ipc2);

	assert_int_equal(0, nmessages);
	assert_string_equal(NULL, message);
	assert_int_equal(0, nmessages2);
	assert_string_equal(NULL, message2);

	assert_string_equal("good result", result);
	free(result);
}

TEST(eval_error_is_handled, IF(enabled_and_not_in_wine))
{
	const char expr[] = "bad expression";
	char *result;

	ipc_t *const ipc1 = ipc_init(NAME, &test_ipc_args, &test_ipc_eval);
	ipc_t *const ipc2 = ipc_init(NAME, &test_ipc_args2, &test_ipc_eval_error);

	assert_success(bg_execute("", "", 0, 1, &other_instance, ipc2));

	result = ipc_eval(ipc1, ipc_get_name(ipc2), expr);
	assert_false(ipc_check(ipc1));

	wait_for_bg();

	ipc_free(ipc1);
	ipc_free(ipc2);

	assert_int_equal(0, nmessages);
	assert_string_equal(NULL, message);
	assert_int_equal(0, nmessages2);
	assert_string_equal(NULL, message2);

	assert_string_equal(NULL, result);
	free(result);
}

TEST(checking_ipc_from_ipc_handler_is_noop, IF(enabled_and_not_windows))
{
	char msg[] = "test message";
	char *data[] = { msg, NULL };

	ipc_t *const ipc1 = ipc_init(NAME, &test_ipc_args, &test_ipc_eval);
	ipc_t *const ipc2 = ipc_init(NAME, &recursive_ipc_args, &test_ipc_eval);

	recursive_ipc = ipc2;

	assert_success(ipc_send(ipc1, ipc_get_name(ipc2), data));
	assert_success(ipc_send(ipc1, ipc_get_name(ipc2), data));
	assert_true(ipc_check(ipc2));
	assert_true(ipc_check(ipc2));

	ipc_free(ipc1);
	ipc_free(ipc2);
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

static void
recursive_ipc_args(char *args[])
{
	assert_false(ipc_check(recursive_ipc));
}

static char *
test_ipc_eval(const char expr[])
{
	if(strcmp("good expression", expr) == 0)
	{
		return strdup("good result");
	}
	return NULL;
}

static char *
test_ipc_eval_error(const char expr[])
{
	return NULL;
}

static void
other_instance(bg_op_t *bg_op, void *arg)
{
	ipc_t *const ipc = arg;
	while(!ipc_check(ipc))
	{
		/* Do nothing. */
	}
}

static int
enabled_and_not_in_wine(void)
{
#ifndef _WIN32
	return ipc_enabled();
#else
	/* Apparently, WINE doesn't implement some things related to named pipes. */
	return (ipc_enabled() && not_wine());
#endif
}

static int
enabled_and_not_windows(void)
{
#ifndef _WIN32
	return ipc_enabled();
#else
	return 0;
#endif
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
