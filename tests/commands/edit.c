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
	init_commands();

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

	(void)exec_commands("%edit", &lwin, CIT_COMMAND);

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

	assert_success(vlua_run_string(curr_stats.vlua,
				"function handler(info)"
				"  local s = ginfo ~= nil"
				"  ginfo = info"
				"  return { success = s }"
				"end"));
	assert_success(vlua_run_string(curr_stats.vlua,
				"vifm.addhandler{ name = 'editor', handler = handler }"));

	int i;
	for(i = 0; i < 2; ++i)
	{
		assert_success(exec_commands("edit a b", &lwin, CIT_COMMAND));

		assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.action)"));
		assert_string_equal("edit-many", ui_sb_last());
		assert_success(vlua_run_string(curr_stats.vlua, "print(#ginfo.paths)"));
		assert_string_equal("2", ui_sb_last());
		assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.paths[1])"));
		assert_string_equal("a", ui_sb_last());
		assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.paths[2])"));
		assert_string_equal("b", ui_sb_last());

		assert_success(vlua_run_string(curr_stats.vlua, "ginfo = {}"));
	}

	vlua_finish(curr_stats.vlua);
	curr_stats.vlua = NULL;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
