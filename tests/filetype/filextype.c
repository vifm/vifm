#include <stic.h>

#include <stdlib.h>

#include "../../src/filetype.h"
#include "../../src/status.h"
#include "test.h"

TEST(one_console_prog)
{
	const char *prog_cmd;

	set_programs("*.tar", "x prog", 1, 0);
	set_programs("*.tar", "console prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("file.version.tar")) != NULL);
	assert_string_equal("console prog", prog_cmd);
}

TEST(one_graphic_prog)
{
	const char *prog_cmd;

	set_programs("*.tar", "x prog", 1, 1);
	set_programs("*.tar", "console prog", 0, 1);

	assert_true((prog_cmd = ft_get_program("file.version.tar")) != NULL);
	assert_string_equal("x prog", prog_cmd);
}

TEST(two_console_prog)
{
	set_programs("*.tgz", "2 x prog", 1, 0);

	assert_null(ft_get_program("file.version.tgz"));
}

TEST(two_graphic_prog)
{
	const char *prog_cmd;

	set_programs("*.tgz", "2 x prog", 1, 1);

	assert_true((prog_cmd = ft_get_program("file.version.tgz")) != NULL);
	assert_string_equal("2 x prog", prog_cmd);
}

TEST(three_console_prog)
{
	const char *prog_cmd;

	set_programs("*.tar.bz2", "3 console prog", 0, 1);

	assert_true((prog_cmd = ft_get_program("file.version.tar.bz2")) != NULL);
	assert_string_equal("3 console prog", prog_cmd);
}

TEST(three_graphic_prog)
{
	const char *prog_cmd;

	set_programs("*.tar.bz2", "3 console prog", 0, 0);

	assert_true((prog_cmd = ft_get_program("file.version.tar.bz2")) != NULL);
	assert_string_equal("3 console prog", prog_cmd);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
