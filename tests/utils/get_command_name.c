#include <stic.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/utils/utils.h"

TEST(empty_command_line)
{
	const char cmd[] = "";
	char command[NAME_MAX + 1];
	const char *args;

	args = extract_cmd_name(cmd, 0, sizeof(command), command);
	assert_string_equal("", command);
	assert_string_equal("", args);
}

TEST(command_name_only)
{
	const char cmd[] = "cmd";
	char command[NAME_MAX + 1];
	const char *args;

	args = extract_cmd_name(cmd, 0, sizeof(command), command);
	assert_string_equal("cmd", command);
	assert_string_equal("", args);
}

TEST(leading_whitespase_ignored)
{
	const char cmd[] = "  \t  cmd";
	char command[NAME_MAX + 1];
	const char *args;

	args = extract_cmd_name(cmd, 0, sizeof(command), command);
	assert_string_equal("cmd", command);
	assert_string_equal("", args);
}

TEST(with_argument_list)
{
	const char cmd[] = "cmd arg1 arg2";
	char command[NAME_MAX + 1];
	const char *args;

	args = extract_cmd_name(cmd, 0, sizeof(command), command);
	assert_string_equal("cmd", command);
	assert_string_equal("arg1 arg2", args);
}

TEST(whitespace_after_command_ignored)
{
	const char cmd[] = "cmd   \t  arg1 arg2";
	char command[NAME_MAX + 1];
	const char *args;

	args = extract_cmd_name(cmd, 0, sizeof(command), command);
	assert_string_equal("cmd", command);
	assert_string_equal("arg1 arg2", args);
}

TEST(fusemount_part_removed)
{
	const char cmd[] = "FUSE_MOUNT|archivemount %SOURCE_FILE %DESTINATION_DIR";
	char command[NAME_MAX + 1];
	const char *args;

	args = extract_cmd_name(cmd, 0, sizeof(command), command);
	assert_string_equal("archivemount", command);
	assert_string_equal("%SOURCE_FILE %DESTINATION_DIR", args);
}

TEST(fusemount2_part_removed)
{
	const char cmd[] = "FUSE_MOUNT2|sshfs %PARAM %DESTINATION_DIR";
	char command[NAME_MAX + 1];
	const char *args;

	args = extract_cmd_name(cmd, 0, sizeof(command), command);
	assert_string_equal("sshfs", command);
	assert_string_equal("%PARAM %DESTINATION_DIR", args);
}

TEST(fusemount3_part_removed)
{
	const char cmd[] = "FUSE_MOUNT3|mount-avfs %DESTINATION_DIR %SOURCE_FILE";
	char command[NAME_MAX + 1];
	const char *args;

	args = extract_cmd_name(cmd, 0, sizeof(command), command);
	assert_string_equal("mount-avfs", command);
	assert_string_equal("%DESTINATION_DIR %SOURCE_FILE", args);
}

#ifdef _WIN32

TEST(quoted_command_raw_ok)
{
	const char cmd[] = "\"quoted cmd\"   \t  arg1 arg2";
	char command[NAME_MAX + 1];
	const char *args;

	args = extract_cmd_name(cmd, 1, sizeof(command), command);
	assert_string_equal("\"quoted cmd\"", command);
	assert_string_equal("arg1 arg2", args);
}

TEST(quoted_command_coocked_ok)
{
	const char cmd[] = "\"quoted cmd\"   \t  arg1 arg2";
	char command[NAME_MAX + 1];
	const char *args;

	args = extract_cmd_name(cmd, 0, sizeof(command), command);
	assert_string_equal("quoted cmd", command);
	assert_string_equal("arg1 arg2", args);
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
