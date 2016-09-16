#include <stic.h>

#include "../../src/int/fuse.h"

TEST(no_macro_string_unchanged)
{
	int foreground;
	char buf[256];
	char format[] = "FUSE_MOUNT|mount";
	char expected[] = "mount";

	format_mount_command("/mnt/point", "/file/path", "param", format, sizeof(buf),
			buf, &foreground);
	assert_int_equal(0, foreground);
	assert_string_equal(expected, buf);
}

TEST(wrong_macro_expanded_to_empty_string)
{
	int foreground;
	char buf[256];
	char format[] = "FUSE_MOUNT|mount %SRC";
	char expected[] = "mount ";

	format_mount_command("/mnt/point", "/file/path", "param", format, sizeof(buf),
			buf, &foreground);
	assert_int_equal(0, foreground);
	assert_string_equal(expected, buf);
}

TEST(foreground_macro_returns_non_zero)
{
	int foreground;
	char buf[256];
	char format[] = "FUSE_MOUNT|mount %FOREGROUND %SRC";
	char expected[] = "mount  ";

	format_mount_command("/mnt/point", "/file/path", "param", format, sizeof(buf),
			buf, &foreground);
	assert_int_equal(1, foreground);
	assert_string_equal(expected, buf);
}

TEST(clear_macro_returns_non_zero)
{
	int foreground;
	char buf[256];
	char format[] = "FUSE_MOUNT|mount %CLEAR %SRC";
	char expected[] = "mount  ";

	format_mount_command("/mnt/point", "/file/path", "param", format, sizeof(buf),
			buf, &foreground);
	assert_int_equal(1, foreground);
	assert_string_equal(expected, buf);
}

TEST(options_before_macros)
{
	int foreground;
	char buf[256];
	char format[] = "FUSE_MOUNT2|mount -o opt1=-,opt2 %PARAM %DESTINATION_DIR";
	char expected[] = "mount -o opt1=-,opt2 param /mnt/point";

	format_mount_command("/mnt/point", "/file/path/dir/file", "param", format,
			sizeof(buf), buf, &foreground);

	assert_int_equal(0, foreground);
	assert_string_equal(expected, buf);
}

TEST(too_long_param)
{
	int foreground;
	char buf[10];
	char format[] = "FUSE_MOUNT2|%PARAM";
	char expected[] = "012345678";

	format_mount_command("a", "b", "0123456789abcdefghijklmnopqrstuvw", format,
			sizeof(buf), buf, &foreground);

	assert_int_equal(0, foreground);
	assert_string_equal(expected, buf);
}

TEST(too_long_destination)
{
	int foreground;
	char buf[10];
	char format[] = "FUSE_MOUNT2|%DESTINATION_DIR";
	char expected[] = "012345678";

	format_mount_command("0123456789abcdefghijklmnopqrstuvw", "y", "z", format,
			sizeof(buf), buf, &foreground);

	assert_int_equal(0, foreground);
	assert_string_equal(expected, buf);
}

TEST(too_long_source)
{
	int foreground;
	char buf[10];
	char format[] = "FUSE_MOUNT2|%SOURCE_FILE";
	char expected[] = "012345678";

	format_mount_command("a", "0123456789abcdefghijklmnopqrstuvw", "z", format,
			sizeof(buf), buf, &foreground);

	assert_int_equal(0, foreground);
	assert_string_equal(expected, buf);
}

TEST(no_bar_symbol)
{
	int foreground;
	char buf[10];
	char format[] = "%SOURCE_FILE\0abc";
	char expected[] = "";

	format_mount_command("a", "m", "z", format, sizeof(buf), buf, &foreground);

	assert_int_equal(0, foreground);
	assert_string_equal(expected, buf);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
