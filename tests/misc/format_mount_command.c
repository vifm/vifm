#include "seatest.h"

#include "../../src/fuse.h"

static void
test_no_macro_string_unchanged(void)
{
	int clear;
	char buf[256];
	char format[] = "FUSE_MOUNT|mount";
	char expected[] = "mount";

	clear = format_mount_command("/mnt/point", "/file/path", "param", format,
			sizeof(buf), buf);
	assert_int_equal(0, clear);
	assert_string_equal(expected, buf);
}

static void
test_wrong_macro_expanded_to_empty_string(void)
{
	int clear;
	char buf[256];
	char format[] = "FUSE_MOUNT|mount %SRC";
	char expected[] = "mount ";

	clear = format_mount_command("/mnt/point", "/file/path", "param", format,
			sizeof(buf), buf);
	assert_int_equal(0, clear);
	assert_string_equal(expected, buf);
}

static void
test_clear_macro_returns_non_zero(void)
{
	int clear;
	char buf[256];
	char format[] = "FUSE_MOUNT|mount %CLEAR %SRC";
	char expected[] = "mount  ";

	clear = format_mount_command("/mnt/point", "/file/path", "param", format,
			sizeof(buf), buf);
	assert_int_equal(1, clear);
	assert_string_equal(expected, buf);
}

static void
test_options_before_macros(void)
{
	int clear;
	char buf[256];
	char format[] = "FUSE_MOUNT2|mount -o opt1=-,opt2 %PARAM %DESTINATION_DIR";
	char expected[] = "mount -o opt1=-,opt2 param /mnt/point";

	clear = format_mount_command("/mnt/point", "/file/path/dir/file", "param",
			format, sizeof(buf), buf);

	assert_int_equal(0, clear);
	assert_string_equal(expected, buf);
}

static void
test_too_long_param(void)
{
	int clear;
	char buf[10];
	char format[] = "FUSE_MOUNT2|%PARAM";
	char expected[] = "012345678";

	clear = format_mount_command("a", "b", "0123456789abcdefghijklmnopqrstuvw",
			format, sizeof(buf), buf);

	assert_int_equal(0, clear);
	assert_string_equal(expected, buf);
}

static void
test_too_long_dest(void)
{
	int clear;
	char buf[10];
	char format[] = "FUSE_MOUNT2|%DESTINATION_DIR";
	char expected[] = "012345678";

	clear = format_mount_command("0123456789abcdefghijklmnopqrstuvw", "y", "z",
			format, sizeof(buf), buf);

	assert_int_equal(0, clear);
	assert_string_equal(expected, buf);
}

static void
test_too_long_source(void)
{
	int clear;
	char buf[10];
	char format[] = "FUSE_MOUNT2|%SOURCE_FILE";
	char expected[] = "012345678";

	clear = format_mount_command("a", "0123456789abcdefghijklmnopqrstuvw", "z",
			format, sizeof(buf), buf);

	assert_int_equal(0, clear);
	assert_string_equal(expected, buf);
}

static void
test_no_bar_symbol(void)
{
	int clear;
	char buf[10];
	char format[] = "%SOURCE_FILE\0abc";
	char expected[] = "";

	clear = format_mount_command("a", "m", "z",
			format, sizeof(buf), buf);

	assert_int_equal(0, clear);
	assert_string_equal(expected, buf);
}

void
format_mount_command_tests(void)
{
	test_fixture_start();

	run_test(test_no_macro_string_unchanged);
	run_test(test_wrong_macro_expanded_to_empty_string);
	run_test(test_clear_macro_returns_non_zero);
	run_test(test_options_before_macros);
	run_test(test_too_long_param);
	run_test(test_too_long_dest);
	run_test(test_too_long_source);
	run_test(test_no_bar_symbol);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
