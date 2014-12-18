#include <stdlib.h>

#include "seatest.h"

#include "../../src/filetype.h"
#include "../../src/status.h"

static void
test_one_pattern(void)
{
	const char *prog_cmd;

	ft_set_programs("*.tar", "tar prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("file.version.tar")) != NULL);
	assert_string_equal("tar prog", prog_cmd);
}

static void
test_many_pattern(void)
{
	const char *prog_cmd;

	ft_set_programs("*.tar", "tar prog", 0, 0);
	ft_set_programs("*.tar.gz", "tar.gz prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("file.version.tar.gz")) != NULL);
	assert_string_equal("tar.gz prog", prog_cmd);
}

static void
test_many_filepattern(void)
{
	const char *prog_cmd;

	ft_set_programs("*.tgz,*.tar.gz", "tar.gz prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("file.version.tar.gz")) != NULL);
	assert_string_equal("tar.gz prog", prog_cmd);
}

static void
test_dont_match_hidden(void)
{
	const char *prog_cmd;

	ft_set_programs("*.tgz,*.tar.gz", "tar.gz prog", 0, 0);

	assert_true((prog_cmd = ft_get_program(".file.version.tar.gz")) == NULL);
}

static void
test_match_empty(void)
{
	const char *prog_cmd;

	ft_set_programs("a*bc", "empty prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("abc")) != NULL);
	assert_string_equal("empty prog", prog_cmd);
}

static void
test_match_full_line(void)
{
	const char *prog_cmd;

	ft_set_programs("abc", "full prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("abcd")) == NULL);
	assert_true((prog_cmd = ft_get_program("0abc")) == NULL);
	assert_true((prog_cmd = ft_get_program("0abcd")) == NULL);

	assert_true((prog_cmd = ft_get_program("abc")) != NULL);
	assert_string_equal("full prog", prog_cmd);
}

static void
test_match_qmark(void)
{
	const char *prog_cmd;

	ft_set_programs("a?c", "full prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("ac")) == NULL);

	assert_true((prog_cmd = ft_get_program("abc")) != NULL);
	assert_string_equal("full prog", prog_cmd);
}

static void
test_qmark_escaping(void)
{
	const char *prog_cmd;

	ft_set_programs("a\\?c", "qmark prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("abc")) == NULL);

	assert_true((prog_cmd = ft_get_program("a?c")) != NULL);
	assert_string_equal("qmark prog", prog_cmd);
}

static void
test_star_escaping(void)
{
	const char *prog_cmd;

	ft_set_programs("a\\*c", "star prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("abc")) == NULL);

	assert_true((prog_cmd = ft_get_program("a*c")) != NULL);
	assert_string_equal("star prog", prog_cmd);
}

static void
test_star_and_dot(void)
{
	const char *prog_cmd;

	ft_set_programs("*.doc", "libreoffice", 0, 0);

	assert_true((prog_cmd = ft_get_program("a.doc")) != NULL);
	assert_string_equal("libreoffice", prog_cmd);

	assert_true((prog_cmd = ft_get_program(".a.doc")) == NULL);
	assert_true((prog_cmd = ft_get_program(".doc")) == NULL);

	ft_set_programs(".*.doc", "hlibreoffice", 0, 0);

	assert_true((prog_cmd = ft_get_program(".a.doc")) != NULL);
	assert_string_equal("hlibreoffice", prog_cmd);
}

static void
test_double_comma(void)
{
	const char *prog_cmd;

	ft_set_programs("*.tar", "prog -o opt1,,opt2", 0, 0);

	assert_true((prog_cmd = ft_get_program("file.version.tar")) != NULL);
	assert_string_equal("prog -o opt1,opt2", prog_cmd);

	ft_set_programs("*.zip", "prog1 -o opt1, prog2", 0, 0);

	assert_true((prog_cmd = ft_get_program("file.version.zip")) != NULL);
	assert_string_equal("prog1 -o opt1", prog_cmd);
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
