#include <stdlib.h>

#include "seatest.h"

#include "../../src/filetype.h"
#include "../../src/status.h"

static void
test_one_pattern(void)
{
	assoc_record_t program;
	int success;

	ft_set_programs("*.tar", "tar prog", 0, 0);

	success = ft_get_program("file.version.tar", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("tar prog", program.command);
		ft_assoc_record_free(&program);
	}
}

static void
test_many_pattern(void)
{
	assoc_record_t program;
	int success;

	ft_set_programs("*.tar", "tar prog", 0, 0);
	ft_set_programs("*.tar.gz", "tar.gz prog", 0, 0);

	success = ft_get_program("file.version.tar.gz", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("tar.gz prog", program.command);
		ft_assoc_record_free(&program);
	}
}

static void
test_many_filepattern(void)
{
	assoc_record_t program;
	int success;

	ft_set_programs("*.tgz,*.tar.gz", "tar.gz prog", 0, 0);

	success = ft_get_program("file.version.tar.gz", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("tar.gz prog", program.command);
		ft_assoc_record_free(&program);
	}
}

static void
test_dont_match_hidden(void)
{
	assoc_record_t program;

	ft_set_programs("*.tgz,*.tar.gz", "tar.gz prog", 0, 0);

	assert_false(ft_get_program(".file.version.tar.gz", &program));
}

static void
test_match_empty(void)
{
	assoc_record_t program;
	int success;

	ft_set_programs("a*bc", "empty prog", 0, 0);

	success = ft_get_program("abc", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("empty prog", program.command);
		ft_assoc_record_free(&program);
	}
}

static void
test_match_full_line(void)
{
	assoc_record_t program;
	int success;

	ft_set_programs("abc", "full prog", 0, 0);

	assert_false(ft_get_program("abcd", &program));
	assert_false(ft_get_program("0abc", &program));
	assert_false(ft_get_program("0abcd", &program));

	success = ft_get_program("abc", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("full prog", program.command);
		ft_assoc_record_free(&program);
	}
}

static void
test_match_qmark(void)
{
	assoc_record_t program;
	int success;

	ft_set_programs("a?c", "full prog", 0, 0);

	assert_false(ft_get_program("ac", &program));

	success = ft_get_program("abc", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("full prog", program.command);
		ft_assoc_record_free(&program);
	}
}

static void
test_qmark_escaping(void)
{
	assoc_record_t program;
	int success;

	ft_set_programs("a\\?c", "qmark prog", 0, 0);

	assert_false(ft_get_program("abc", &program));

	success = ft_get_program("a?c", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("qmark prog", program.command);
		ft_assoc_record_free(&program);
	}
}

static void
test_star_escaping(void)
{
	assoc_record_t program;
	int success;

	ft_set_programs("a\\*c", "star prog", 0, 0);

	assert_false(ft_get_program("abc", &program));

	success = ft_get_program("a*c", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("star prog", program.command);
		ft_assoc_record_free(&program);
	}
}

static void
test_star_and_dot(void)
{
	assoc_record_t program;
	int success;

	ft_set_programs("*.doc", "libreoffice", 0, 0);

	success = ft_get_program("a.doc", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("libreoffice", program.command);
		ft_assoc_record_free(&program);
	}

	assert_false(ft_get_program(".a.doc", &program));
	assert_false(ft_get_program(".doc", &program));

	ft_set_programs(".*.doc", "hlibreoffice", 0, 0);

	success = ft_get_program(".a.doc", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("hlibreoffice", program.command);
		ft_assoc_record_free(&program);
	}
}

static void
test_double_comma(void)
{
	assoc_record_t program;
	int success;

	ft_set_programs("*.tar", "prog -o opt1,,opt2", 0, 0);

	success = ft_get_program("file.version.tar", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("prog -o opt1,opt2", program.command);
		ft_assoc_record_free(&program);
	}

	ft_set_programs("*.zip", "prog1 -o opt1, prog2", 0, 0);

	success = ft_get_program("file.version.zip", &program);
	assert_true(success);
	if(success)
	{
		assert_string_equal("prog1 -o opt1", program.command);
		ft_assoc_record_free(&program);
	}
}

void
filetype_tests(void)
{
	test_fixture_start();

	run_test(test_one_pattern);
	run_test(test_many_pattern);
	run_test(test_many_filepattern);
	run_test(test_dont_match_hidden);
	run_test(test_match_empty);
	run_test(test_match_full_line);
	run_test(test_match_qmark);
	run_test(test_qmark_escaping);
	run_test(test_star_escaping);
	run_test(test_double_comma);

	run_test(test_star_and_dot);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
