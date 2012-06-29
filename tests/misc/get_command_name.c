#include "seatest.h"

#include "../../src/utils/utils.h"

static void
test_empty_command_line(void)
{
	char cmd[] = "";
	char command[NAME_MAX];
	const char *args;

	args = get_command_name(cmd, 0, sizeof(command), command);
	assert_string_equal("", command);
	assert_string_equal("", args);
}

static void
test_command_name_only(void)
{
	char cmd[] = "cmd";
	char command[NAME_MAX];
	const char *args;

	args = get_command_name(cmd, 0, sizeof(command), command);
	assert_string_equal("cmd", command);
	assert_string_equal("", args);
}

static void
test_leading_whitespase_ignored(void)
{
	char cmd[] = "  \t  cmd";
	char command[NAME_MAX];
	const char *args;

	args = get_command_name(cmd, 0, sizeof(command), command);
	assert_string_equal("cmd", command);
	assert_string_equal("", args);
}

static void
test_with_argument_list(void)
{
	char cmd[] = "cmd arg1 arg2";
	char command[NAME_MAX];
	const char *args;

	args = get_command_name(cmd, 0, sizeof(command), command);
	assert_string_equal("cmd", command);
	assert_string_equal("arg1 arg2", args);
}

static void
test_whitespace_after_command_ignored(void)
{
	char cmd[] = "cmd   \t  arg1 arg2";
	char command[NAME_MAX];
	const char *args;

	args = get_command_name(cmd, 0, sizeof(command), command);
	assert_string_equal("cmd", command);
	assert_string_equal("arg1 arg2", args);
}

#ifdef _WIN32
static void
test_quoted_command_raw_ok(void)
{
	char cmd[] = "\"quoted cmd\"   \t  arg1 arg2";
	char command[NAME_MAX];
	const char *args;

	args = get_command_name(cmd, 1, sizeof(command), command);
	assert_string_equal("\"quoted cmd\"", command);
	assert_string_equal("arg1 arg2", args);
}

static void
test_quoted_command_coocked_ok(void)
{
	char cmd[] = "\"quoted cmd\"   \t  arg1 arg2";
	char command[NAME_MAX];
	const char *args;

	args = get_command_name(cmd, 0, sizeof(command), command);
	assert_string_equal("quoted cmd", command);
	assert_string_equal("arg1 arg2", args);
}
#endif

void
get_command_name_tests(void)
{
	test_fixture_start();

	run_test(test_empty_command_line);
	run_test(test_command_name_only);
	run_test(test_leading_whitespase_ignored);
	run_test(test_with_argument_list);
	run_test(test_whitespace_after_command_ignored);
#ifdef _WIN32
	run_test(test_quoted_command_raw_ok);
	run_test(test_quoted_command_coocked_ok);
#endif

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
