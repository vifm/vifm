#include <stic.h>

#include <stdio.h> /* FILE fclose() fopen() fprintf() remove() */
#include <string.h> /* strcpy() strdup() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/status.h"
#include "../lua/asserts.h"

static char sandbox[PATH_MAX + 1];

SETUP_ONCE()
{
	char cwd[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	make_abs_path(sandbox, sizeof(sandbox), SANDBOX_PATH, "", cwd);
}

SETUP()
{
	conf_setup();
	view_setup(&lwin);
	cmds_init();

	curr_view = &lwin;
}

TEARDOWN()
{
	conf_teardown();
	view_teardown(&lwin);
	vle_cmds_reset();

	curr_view = NULL;
}

TEST(edit_handles_ranges, IF(not_windows))
{
	create_file(SANDBOX_PATH "/file1");
	create_file(SANDBOX_PATH "/file2");

	char script_path[PATH_MAX + 1];
	make_abs_path(script_path, sizeof(script_path), sandbox, "script", NULL);
	update_string(&cfg.vi_command, script_path);
	update_string(&cfg.vi_x_command, "");

	FILE *fp = fopen(SANDBOX_PATH "/script", "w");
	fprintf(fp, "#!/bin/sh\n");
	fprintf(fp, "for arg; do echo \"$arg\" >> %s/vi-list; done\n", SANDBOX_PATH);
	fclose(fp);
	assert_success(os_chmod(SANDBOX_PATH "/script", 0777));

	strcpy(lwin.curr_dir, sandbox);
	lwin.list_rows = 2;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("file1");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].name = strdup("file2");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];

	(void)cmds_dispatch("%edit", &lwin, CIT_COMMAND);

	const char *lines[] = { "file1", "file2" };
	file_is(SANDBOX_PATH "/vi-list", lines, ARRAY_LEN(lines));

	assert_success(remove(SANDBOX_PATH "/script"));
	assert_success(remove(SANDBOX_PATH "/file1"));
	assert_success(remove(SANDBOX_PATH "/file2"));
	assert_success(remove(SANDBOX_PATH "/vi-list"));
}

TEST(edit_command)
{
	curr_stats.exec_env_type = EET_EMULATOR;
	update_string(&cfg.vi_command, "#vifmtest#editor");
	cfg.config_dir[0] = '\0';

	curr_stats.vlua = vlua_init();

	GLUA_EQ(curr_stats.vlua, "",
			"function handler(info)"
			"  local s = ginfo ~= nil"
			"  ginfo = info"
			"  return { success = s }"
			"end");
	GLUA_EQ(curr_stats.vlua, "",
			"vifm.addhandler{ name = 'editor', handler = handler }");

	int i;
	for(i = 0; i < 2; ++i)
	{
		assert_success(cmds_dispatch("edit a b", &lwin, CIT_COMMAND));

		GLUA_EQ(curr_stats.vlua, "edit-many", "print(ginfo.action)");
		GLUA_EQ(curr_stats.vlua, "2", "print(#ginfo.paths)");
		GLUA_EQ(curr_stats.vlua, "a", "print(ginfo.paths[1])");
		GLUA_EQ(curr_stats.vlua, "b", "print(ginfo.paths[2])");

		GLUA_EQ(curr_stats.vlua, "", "ginfo = {}");
	}

	vlua_finish(curr_stats.vlua);
	curr_stats.vlua = NULL;
}

TEST(edit_broken_symlink, IF(not_windows))
{
	make_symlink("no-such-file", SANDBOX_PATH "/broken");

	update_string(&cfg.vi_command, "rm");
	update_string(&cfg.vi_x_command, "");

	char *saved_cwd = save_cwd();
	assert_success(chdir(sandbox));

	strcpy(lwin.curr_dir, sandbox);
	lwin.list_rows = 1;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("broken");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];

	(void)cmds_dispatch("edit", &lwin, CIT_COMMAND);

	restore_cwd(saved_cwd);
	remove_file(SANDBOX_PATH "/broken");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
