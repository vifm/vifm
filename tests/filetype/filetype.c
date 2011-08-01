#include <stdlib.h>

#include "seatest.h"

#include "../../src/filetype.h"

static void
test_one_ext(void)
{
	char *buf;

	set_programs("*.tar", "tar prog");

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

	set_programs("*.tar", "tar prog");
	set_programs("*.tar.gz", "tar.gz prog");

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

	set_programs("*.tgz,*.tar.gz", "tar.gz prog");

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

	set_programs("*.tgz,*.tar.gz", "tar.gz prog");

	buf = get_default_program_for_file(".file.version.tar.gz");
	assert_true(buf == NULL);
}

static void
test_match_empty(void)
{
	char *buf;

	set_programs("a*bc", "empty prog");

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

	set_programs("abc", "full prog");

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

static void
test_match_qmark(void)
{
	char *buf;

	set_programs("a?c", "full prog");

	buf = get_default_program_for_file("ac");
	assert_true(buf == NULL);

	buf = get_default_program_for_file("abc");
	assert_false(buf == NULL);
	if(buf != NULL)
		assert_string_equal("full prog", buf);
	free(buf);
}

static void
test_escaping(void)
{
	char *buf;

	set_programs("a\\?c", "qmark prog");

	buf = get_default_program_for_file("abc");
	assert_true(buf == NULL);

	buf = get_default_program_for_file("a?c");
	assert_false(buf == NULL);
	if(buf != NULL)
		assert_string_equal("qmark prog", buf);
	free(buf);

	set_programs("a\\*c", "star prog");

	buf = get_default_program_for_file("abc");
	assert_true(buf == NULL);

	buf = get_default_program_for_file("a*c");
	assert_false(buf == NULL);
	if(buf != NULL)
		assert_string_equal("star prog", buf);
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
	run_test(test_match_qmark);
	run_test(test_escaping);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
