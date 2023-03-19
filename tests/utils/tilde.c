#include <stic.h>

#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strcpy() */

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/utils/path.h"

TEST(no_tilde)
{
	char *expanded = expand_tilde("/no/tilde");
	assert_string_equal("/no/tilde", expanded);
	free(expanded);
}

TEST(just_the_tilde)
{
	strcpy(cfg.home_dir, "/homedir/");

	char *expanded = expand_tilde("~");
	assert_string_equal("/homedir/", expanded);
	free(expanded);
}

TEST(tilde_and_slash)
{
	strcpy(cfg.home_dir, "/homedir/");

	char *expanded = expand_tilde("~/");
	assert_string_equal("/homedir/", expanded);
	free(expanded);
}

TEST(tilde_and_path)
{
	strcpy(cfg.home_dir, "/homedir/");

	char *expanded = expand_tilde("~/path");
	assert_string_equal("/homedir/path", expanded);
	free(expanded);
}

TEST(invalid_user_name)
{
	char *expanded = expand_tilde("~6r|o_o-t0#a!!!");
	assert_string_equal("~6r|o_o-t0#a!!!", expanded);
	free(expanded);
}

TEST(huge_user_name_after_tilde)
{
	char path[PATH_MAX*2 + 1];
	snprintf(path, sizeof(path), "~%*s/", (int)sizeof(path) - 7, "name");

	char *expanded = expand_tilde(path);
	assert_string_equal(path, expanded);
	free(expanded);
}
