#include <stdlib.h>

#include "seatest.h"

#include "../../src/filetype.h"
#include "../../src/status.h"

static void
test_one_pattern(void)
{
	assoc_prog_t program;

	set_programs("*.tar", "tar prog", 0);

	assert_true(get_default_program_for_file("file.version.tar", &program));
	if(program.com != NULL)
		assert_string_equal("tar prog", program.com);
	free_assoc_prog(&program);
}

static void
test_many_pattern(void)
{
	assoc_prog_t program;

	set_programs("*.tar", "tar prog", 0);
	set_programs("*.tar.gz", "tar.gz prog", 0);

	assert_true(get_default_program_for_file("file.version.tar.gz", &program));
	if(program.com != NULL)
		assert_string_equal("tar.gz prog", program.com);
	free_assoc_prog(&program);
}

static void
test_many_filepattern(void)
{
	assoc_prog_t program;

	set_programs("*.tgz,*.tar.gz", "tar.gz prog", 0);

	assert_true(get_default_program_for_file("file.version.tar.gz", &program));
	if(program.com != NULL)
		assert_string_equal("tar.gz prog", program.com);
	free_assoc_prog(&program);
}

static void
test_dont_match_hidden(void)
{
	assoc_prog_t program;

	set_programs("*.tgz,*.tar.gz", "tar.gz prog", 0);

	assert_false(get_default_program_for_file(".file.version.tar.gz", &program));
}

static void
test_match_empty(void)
{
	assoc_prog_t program;

	set_programs("a*bc", "empty prog", 0);

	assert_true(get_default_program_for_file("abc", &program));
	if(program.com != NULL)
		assert_string_equal("empty prog", program.com);
	free_assoc_prog(&program);
}

static void
test_match_full_line(void)
{
	assoc_prog_t program;

	set_programs("abc", "full prog", 0);

	assert_false(get_default_program_for_file("abcd", &program));
	assert_false(get_default_program_for_file("0abc", &program));
	assert_false(get_default_program_for_file("0abcd", &program));

	assert_true(get_default_program_for_file("abc", &program));
	if(program.com != NULL)
		assert_string_equal("full prog", program.com);
	free_assoc_prog(&program);
}

static void
test_match_qmark(void)
{
	assoc_prog_t program;

	set_programs("a?c", "full prog", 0);

	assert_false(get_default_program_for_file("ac", &program));

	assert_true(get_default_program_for_file("abc", &program));
	if(program.com != NULL)
		assert_string_equal("full prog", program.com);
	free_assoc_prog(&program);
}

static void
test_qmark_escaping(void)
{
	assoc_prog_t program;

	set_programs("a\\?c", "qmark prog", 0);

	assert_false(get_default_program_for_file("abc", &program));

	assert_true(get_default_program_for_file("a?c", &program));
	if(program.com != NULL)
		assert_string_equal("qmark prog", program.com);
	free_assoc_prog(&program);
}

static void
test_star_escaping(void)
{
	assoc_prog_t program;

	set_programs("a\\*c", "star prog", 0);

	assert_false(get_default_program_for_file("abc", &program));

	assert_true(get_default_program_for_file("a*c", &program));
	if(program.com != NULL)
		assert_string_equal("star prog", program.com);
	free_assoc_prog(&program);
}

static void
test_star_and_dot(void)
{
	assoc_prog_t program;

	curr_stats.is_console = 1;
	set_programs("*.doc", "libreoffice", 0);

	assert_true(get_default_program_for_file("a.doc", &program));
	if(program.com != NULL)
		assert_string_equal("libreoffice", program.com);
	free_assoc_prog(&program);

	assert_false(get_default_program_for_file(".a.doc", &program));
	assert_false(get_default_program_for_file(".doc", &program));

	set_programs(".*.doc", "hlibreoffice", 0);

	assert_true(get_default_program_for_file(".a.doc", &program));
	if(program.com != NULL)
		assert_string_equal("hlibreoffice", program.com);
	free_assoc_prog(&program);
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

	run_test(test_star_and_dot);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
