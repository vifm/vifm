#include <stic.h>

#include <unistd.h> /* chdir() */

#include <stddef.h> /* NULL */
#include <stdio.h> /* snprintf() */
#include <string.h> /* strdup() */

#include "../../src/compat/fs_limits.h"
#include "../../src/cfg/config.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/matchers.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/filetype.h"
#include "../../src/running.h"
#include "../../src/status.h"

static int prog_exists(const char name[]);

SETUP()
{
#ifndef _WIN32
	update_string(&cfg.shell, "/bin/sh");
	update_string(&cfg.shell_cmd_flag, "-c");
#else
	update_string(&cfg.shell, "cmd");
	update_string(&cfg.shell_cmd_flag, "/C");
#endif

	update_string(&cfg.vi_command, "echo");

	stats_update_shell_type(cfg.shell);

	lwin.list_rows = 2;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("a");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].name = strdup("b");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].selected = 1;
	lwin.selected_files = 2;
	curr_view = &lwin;

	ft_init(&prog_exists);

	if(is_path_absolute(TEST_DATA_PATH))
	{
		snprintf(lwin.curr_dir, sizeof(lwin.curr_dir), "%s/existing-files",
				TEST_DATA_PATH);
	}
	else
	{
		char cwd[PATH_MAX + 1];
		assert_non_null(get_cwd(cwd, sizeof(cwd)));

		snprintf(lwin.curr_dir, sizeof(lwin.curr_dir), "%s/%s/existing-files", cwd,
				TEST_DATA_PATH);
	}
}

TEARDOWN()
{
	update_string(&cfg.shell, NULL);
	update_string(&cfg.shell_cmd_flag, NULL);
	stats_update_shell_type("/bin/sh");

	int i;
	for(i = 0; i < lwin.list_rows; ++i)
	{
		fentry_free(&lwin, &lwin.dir_entry[i]);
	}
	dynarray_free(lwin.dir_entry);

	ft_reset(0);
	update_string(&cfg.vi_command, NULL);
}

TEST(full_path_regexps_are_handled_for_selection)
{
	matchers_t *ms;
	char pattern[PATH_MAX + 16];
	char *error;

	/* Mind that there is no chdir(), this additionally checks that origins are
	 * being used by the code. */

	snprintf(pattern, sizeof(pattern), "//%s/*//", lwin.curr_dir);
	ms = matchers_alloc(pattern, 0, 1, "", &error);
	assert_non_null(ms);
	ft_set_programs(ms, "echo %f >> " SANDBOX_PATH "/run", 0, 1);

	open_file(&lwin, FHE_NO_RUN);

	/* Checking for file existence on its removal. */
	assert_success(remove(SANDBOX_PATH "/run"));
}

TEST(full_path_regexps_are_handled_for_selection2)
{
	matchers_t *ms;
	char pattern[PATH_MAX + 16];
	char *error;

	/* Mind that there is no chdir(), this additionally checks that origins are
	 * being used by the code. */

	snprintf(pattern, sizeof(pattern), "//%s/*//", lwin.curr_dir);
	ms = matchers_alloc(pattern, 0, 1, "", &error);
	assert_non_null(ms);
#ifndef _WIN32
	ft_set_programs(ms, "echo > /dev/null %c &", 0, 1);
#else
	ft_set_programs(ms, "echo > NUL %c &", 0, 1);
#endif

	open_file(&lwin, FHE_NO_RUN);

	/* If we don't crash, then everything is fine. */
}

static int
prog_exists(const char name[])
{
	return 1;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
