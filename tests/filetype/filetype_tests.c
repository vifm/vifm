#include <stic.h>

#include <stdlib.h>

#include "../../src/filetype.h"
#include "../../src/status.h"

TEST(one_pattern)
{
	const char *prog_cmd;

	ft_set_programs("*.tar", 1, "tar prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("file.version.tar")) != NULL);
	assert_string_equal("tar prog", prog_cmd);
}

TEST(many_pattern)
{
	const char *prog_cmd;

	ft_set_programs("*.tar", 1, "tar prog", 0, 0);
	ft_set_programs("*.tar.gz", 1, "tar.gz prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("file.version.tar.gz")) != NULL);
	assert_string_equal("tar.gz prog", prog_cmd);
}

TEST(many_filepattern)
{
	const char *prog_cmd;

	ft_set_programs("*.tgz,*.tar.gz", 1, "tar.gz prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("file.version.tar.gz")) != NULL);
	assert_string_equal("tar.gz prog", prog_cmd);
}

TEST(dont_match_hidden)
{
	const char *prog_cmd;

	ft_set_programs("*.tgz,*.tar.gz", 1, "tar.gz prog", 0, 0);

	assert_true((prog_cmd = ft_get_program(".file.version.tar.gz")) == NULL);
}

TEST(match_empty)
{
	const char *prog_cmd;

	ft_set_programs("a*bc", 1, "empty prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("abc")) != NULL);
	assert_string_equal("empty prog", prog_cmd);
}

TEST(match_full_line)
{
	const char *prog_cmd;

	ft_set_programs("abc", 1, "full prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("abcd")) == NULL);
	assert_true((prog_cmd = ft_get_program("0abc")) == NULL);
	assert_true((prog_cmd = ft_get_program("0abcd")) == NULL);

	assert_true((prog_cmd = ft_get_program("abc")) != NULL);
	assert_string_equal("full prog", prog_cmd);
}

TEST(match_qmark)
{
	const char *prog_cmd;

	ft_set_programs("a?c", 1, "full prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("ac")) == NULL);

	assert_true((prog_cmd = ft_get_program("abc")) != NULL);
	assert_string_equal("full prog", prog_cmd);
}

TEST(qmark_escaping)
{
	const char *prog_cmd;

	ft_set_programs("a\\?c", 1, "qmark prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("abc")) == NULL);

	assert_true((prog_cmd = ft_get_program("a?c")) != NULL);
	assert_string_equal("qmark prog", prog_cmd);
}

TEST(star_escaping)
{
	const char *prog_cmd;

	ft_set_programs("a\\*c", 1, "star prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("abc")) == NULL);

	assert_true((prog_cmd = ft_get_program("a*c")) != NULL);
	assert_string_equal("star prog", prog_cmd);
}

TEST(star_and_dot)
{
	const char *prog_cmd;

	ft_set_programs("*.doc", 1, "libreoffice", 0, 0);

	assert_true((prog_cmd = ft_get_program("a.doc")) != NULL);
	assert_string_equal("libreoffice", prog_cmd);

	assert_true((prog_cmd = ft_get_program(".a.doc")) == NULL);
	assert_true((prog_cmd = ft_get_program(".doc")) == NULL);

	ft_set_programs(".*.doc", 1, "hlibreoffice", 0, 0);

	assert_true((prog_cmd = ft_get_program(".a.doc")) != NULL);
	assert_string_equal("hlibreoffice", prog_cmd);
}

TEST(double_comma)
{
	const char *prog_cmd;

	ft_set_programs("*.tar", 1, "prog -o opt1,,opt2", 0, 0);

	assert_true((prog_cmd = ft_get_program("file.version.tar")) != NULL);
	assert_string_equal("prog -o opt1,opt2", prog_cmd);

	ft_set_programs("*.zip", 1, "prog1 -o opt1, prog2", 0, 0);

	assert_true((prog_cmd = ft_get_program("file.version.zip")) != NULL);
	assert_string_equal("prog1 -o opt1", prog_cmd);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
