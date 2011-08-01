#include <stdlib.h>

#include "seatest.h"

#include "../../src/filetype.h"

static void
test_one_ext(void)
{
	char *buf;

	add_filetype("tar", "*.tar", "tar prog");

	buf = get_default_program_for_file("file.version.tar");
	assert_false(buf == NULL);
	if(buf != NULL)
		assert_string_equal("tar prog", buf);
	free(buf);
}

static void
test_many_ext(void)
{
	char *buf;

	add_filetype("tar", "*.tar", "tar prog");
	add_filetype("tar.gz", "*.tar.gz", "tar.gz prog");

	buf = get_default_program_for_file("file.version.tar.gz");
	assert_false(buf == NULL);
	if(buf != NULL)
		assert_string_equal("tar.gz prog", buf);
	free(buf);
}

static void
test_many_fileext(void)
{
	char *buf;

	add_filetype("tgz,tar.gz", "*.tgz,*.tar.gz", "tar.gz prog");

	buf = get_default_program_for_file("file.version.tar.gz");
	assert_false(buf == NULL);
	if(buf != NULL)
		assert_string_equal("tar.gz prog", buf);
	free(buf);
}

static void
test_dont_match_hidden(void)
{
	char *buf;

	add_filetype("tgz,tar.gz", "*.tgz,*.tar.gz", "tar.gz prog");

	buf = get_default_program_for_file(".file.version.tar.gz");
	assert_true(buf == NULL);
}

static void
test_match_empty(void)
{
	char *buf;

	add_filetype("empty", "a*bc", "empty prog");

	buf = get_default_program_for_file("abc");
	assert_false(buf == NULL);
	if(buf != NULL)
		assert_string_equal("empty prog", buf);
	free(buf);
}

static void
test_match_full_line(void)
{
	char *buf;

	add_filetype("full", "abc", "full prog");

	buf = get_default_program_for_file("abcd");
	assert_true(buf == NULL);

	buf = get_default_program_for_file("0abc");
	assert_true(buf == NULL);

	buf = get_default_program_for_file("0abcd");
	assert_true(buf == NULL);

	buf = get_default_program_for_file("abc");
	assert_false(buf == NULL);
	if(buf != NULL)
		assert_string_equal("full prog", buf);
	free(buf);
}

void
filetype_tests(void)
{
	test_fixture_start();

	run_test(test_one_ext);
	run_test(test_many_ext);
	run_test(test_many_fileext);
	run_test(test_dont_match_hidden);
	run_test(test_match_empty);
	run_test(test_match_full_line);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
