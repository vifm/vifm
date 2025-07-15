#include <stic.h>

#include <stdlib.h>

#include <test-utils.h>

#include "../../src/int/file_magic.h"
#include "../../src/filetype.h"
#include "../../src/status.h"

#include "test.h"

TEST(one_pattern)
{
	const char *prog_cmd;

	assoc_programs("*.tar", "tar prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("file.version.tar")) != NULL);
	assert_string_equal("tar prog", prog_cmd);
}

TEST(many_pattern)
{
	const char *prog_cmd;

	assoc_programs("*.tar", "tar prog", 0, 0);
	assoc_programs("*.tar.gz", "tar.gz prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("file.version.tar.gz")) != NULL);
	assert_string_equal("tar.gz prog", prog_cmd);
}

TEST(many_filepattern)
{
	const char *prog_cmd;

	assoc_programs("*.tgz,*.tar.gz", "tar.gz prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("file.version.tar.gz")) != NULL);
	assert_string_equal("tar.gz prog", prog_cmd);
}

TEST(dont_match_hidden)
{
	assoc_programs("*.tgz,*.tar.gz", "tar.gz prog", 0, 0);

	assert_null(ft_get_program(".file.version.tar.gz"));
}

TEST(match_empty)
{
	const char *prog_cmd;

	assoc_programs("a*bc", "empty prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("abc")) != NULL);
	assert_string_equal("empty prog", prog_cmd);
}

TEST(match_full_line)
{
	const char *prog_cmd;

	assoc_programs("abc", "full prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("abcd")) == NULL);
	assert_true((prog_cmd = ft_get_program("0abc")) == NULL);
	assert_true((prog_cmd = ft_get_program("0abcd")) == NULL);

	assert_true((prog_cmd = ft_get_program("abc")) != NULL);
	assert_string_equal("full prog", prog_cmd);
}

TEST(match_qmark)
{
	const char *prog_cmd;

	assoc_programs("a?c", "full prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("ac")) == NULL);

	assert_true((prog_cmd = ft_get_program("abc")) != NULL);
	assert_string_equal("full prog", prog_cmd);
}

TEST(qmark_escaping)
{
	const char *prog_cmd;

	assoc_programs("a\\?c", "qmark prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("abc")) == NULL);

	assert_true((prog_cmd = ft_get_program("a?c")) != NULL);
	assert_string_equal("qmark prog", prog_cmd);
}

TEST(star_escaping)
{
	const char *prog_cmd;

	assoc_programs("a\\*c", "star prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("abc")) == NULL);

	assert_true((prog_cmd = ft_get_program("a*c")) != NULL);
	assert_string_equal("star prog", prog_cmd);
}

TEST(star_and_dot)
{
	const char *prog_cmd;

	assoc_programs(".xls,*.doc", "libreoffice", 0, 0);

	assert_true((prog_cmd = ft_get_program("a.doc")) != NULL);
	assert_string_equal("libreoffice", prog_cmd);

	assert_true((prog_cmd = ft_get_program(".a.doc")) == NULL);
	assert_true((prog_cmd = ft_get_program(".doc")) == NULL);

	assoc_programs(".*.doc", "hlibreoffice", 0, 0);

	assert_true((prog_cmd = ft_get_program(".a.doc")) != NULL);
	assert_string_equal("hlibreoffice", prog_cmd);
}

TEST(double_comma_in_command)
{
	const char *prog_cmd;

	assoc_programs("*.tar", "prog -o opt1,,opt2", 0, 0);

	assert_true((prog_cmd = ft_get_program("file.version.tar")) != NULL);
	assert_string_equal("prog -o opt1,opt2", prog_cmd);

	assoc_programs("*.zip", "prog1 -o opt1, prog2", 0, 0);

	assert_true((prog_cmd = ft_get_program("file.version.zip")) != NULL);
	assert_string_equal("prog1 -o opt1", prog_cmd);
}

TEST(double_comma_in_pattern)
{
	assoc_programs("a,,b,*.tar", "prog1", 0, 0);
	assert_string_equal("prog1", ft_get_program("a,b"));
	assert_string_equal("prog1", ft_get_program("file.version.tar"));

	assoc_programs("{c,,d}", "prog2", 0, 0);
	assert_string_equal("prog2", ft_get_program("c,d"));
}

TEST(zero_length_match)
{
	/* This is confirmation of limitation of globs->regex transition.  There is a
	 * complication for list of globs, so it's left with somewhat incorrect
	 * behaviour. */

	const char *prog_cmd;

	assoc_programs("*git", "tig", 0, 0);

	assert_null(prog_cmd = ft_get_program("git"));
}

TEST(existence_check)
{
	assoc_programs("*.git", "tig", 0, 0);

	assert_false(ft_assoc_exists(&filetypes, "git", "tig"));

	assert_false(ft_assoc_exists(&filetypes, "*.git", "ti"));
	assert_false(ft_assoc_exists(&filetypes, "*.git", "{tig"));
	assert_false(ft_assoc_exists(&filetypes, "*.git", "{tig}"));

	assert_true(ft_assoc_exists(&filetypes, "*.git", "tig"));
	assert_true(ft_assoc_exists(&filetypes, "*.git", "{}tig"));
	assert_true(ft_assoc_exists(&filetypes, "*.git", "{help text}tig"));
	assert_true(ft_assoc_exists(&filetypes, "*.git", "{help text}\t tig"));
}

TEST(pattern_list, IF(has_mime_type_detection))
{
	char cmd[1024];

	snprintf(cmd, sizeof(cmd), "<%s>{binary-data}",
			get_mimetype(TEST_DATA_PATH "/read/binary-data", 0));
	assoc_programs(cmd, "prog", 0, 0);

	assert_string_equal("prog",
			ft_get_program(TEST_DATA_PATH "/read/binary-data"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
