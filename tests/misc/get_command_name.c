#include "seatest.h"

#include "../../src/utils/fs_limits.h"
#include "../../src/utils/utils.h"

static void
test_empty_command_line(void)
{
	const char cmd[] = "";
	char command[NAME_MAX];
	const char *args;

	args = extract_cmd_name(cmd, 0, sizeof(command), command);
	assert_string_equal("", command);
	assert_string_equal("", args);
}

static void
test_command_name_only(void)
{
	const char cmd[] = "cmd";
	char command[NAME_MAX];
	const char *args;

	args = extract_cmd_name(cmd, 0, sizeof(command), command);
	assert_string_equal("cmd", command);
	assert_string_equal("", args);
}

static void
test_leading_whitespase_ignored(void)
{
	const char cmd[] = "  \t  cmd";
	char command[NAME_MAX];
	const char *args;

	args = extract_cmd_name(cmd, 0, sizeof(command), command);
	assert_string_equal("cmd", command);
	assert_string_equal("", args);
}

static void
test_with_argument_list(void)
{
	const char cmd[] = "cmd arg1 arg2";
	char command[NAME_MAX];
	const char *args;

	args = extract_cmd_name(cmd, 0, sizeof(command), command);
	assert_string_equal("cmd", command);
	assert_string_equal("arg1 arg2", args);
}

static void
test_whitespace_after_command_ignored(void)
{
	const char cmd[] = "cmd   \t  arg1 arg2";
	char command[NAME_MAX];
	const char *args;

	args = extract_cmd_name(cmd, 0, sizeof(command), command);
	assert_string_equal("cmd", command);
	assert_string_equal("arg1 arg2", args);
}

static void
test_fusemount_part_removed(void)
{
	const char cmd[] = "FUSE_MOUNT|archivemount %SOURCE_FILE %DESTINATION_DIR";
	char command[NAME_MAX];
	const char *args;

	args = extract_cmd_name(cmd, 0, sizeof(command), command);
	assert_string_equal("archivemount", command);
	assert_string_equal("%SOURCE_FILE %DESTINATION_DIR", args);
}

static void
test_fusemount2_part_removed(void)
{
	const char cmd[] = "FUSE_MOUNT2|sshfs %PARAM %DESTINATION_DIR";
	char command[NAME_MAX];
	const char *args;

	args = extract_cmd_name(cmd, 0, sizeof(command), command);
	assert_string_equal("sshfs", command);
	assert_string_equal("%PARAM %DESTINATION_DIR", args);
}

#ifdef _WIN32
static void
test_quoted_command_raw_ok(void)
{
	const char cmd[] = "\"quoted cmd\"   \t  arg1 arg2";
	char command[NAME_MAX];
	const char *args;

	args = extract_cmd_name(cmd, 1, sizeof(command), command);
	assert_string_equal("\"quoted cmd\"", command);
	assert_string_equal("arg1 arg2", args);
}

static void
test_quoted_command_coocked_ok(void)
{
	const char cmd[] = "\"quoted cmd\"   \t  arg1 arg2";
	char command[NAME_MAX];
	const char *args;

	args = extract_cmd_name(cmd, 0, sizeof(command), command);
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
	run_test(test_fusemount_part_removed);
	run_test(test_fusemount2_part_removed);
#ifdef _WIN32
	run_test(test_quoted_command_raw_ok);
	run_test(test_quoted_command_coocked_ok);
#endif

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
