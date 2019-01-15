#ifndef _WIN32

#include <stic.h>

#include <unistd.h> /* chdir() */

#include <stddef.h> /* NULL */
#include <stdio.h> /* remove() */
#include <string.h> /* strcmp() strcpy() */

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/modes/menu.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/utils/string_array.h"
#include "../../src/cmd_core.h"
#include "../../src/status.h"

#include "utils.h"

static char sandbox[PATH_MAX + 1];
static char *saved_cwd;

SETUP_ONCE()
{
	char cwd[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
	make_abs_path(sandbox, sizeof(sandbox), SANDBOX_PATH, "", cwd);
}

SETUP()
{
	conf_setup();
	init_modes();
	init_commands();

	curr_view = &lwin;
	other_view = &rwin;
	view_setup(&lwin);

	curr_stats.load_stage = -1;
	curr_stats.save_msg = 0;

	saved_cwd = save_cwd();
	assert_success(chdir(sandbox));

	update_string(&cfg.shell, "/bin/sh");
	update_string(&cfg.shell_cmd_flag, "-c");
	stats_update_shell_type(cfg.shell);
	curr_stats.exec_env_type = EET_EMULATOR;

	update_string(&cfg.media_prg, "./script");

	create_executable("script");
}

TEARDOWN()
{
	assert_success(remove("script"));

	restore_cwd(saved_cwd);

	vle_keys_reset();
	conf_teardown();

	view_teardown(&lwin);
	curr_view = NULL;
	other_view = NULL;

	curr_stats.load_stage = 0;
}

TEST(menu_aborts_if_mediaprg_is_not_set)
{
	update_string(&cfg.media_prg, "");

	assert_success(exec_commands("media", &lwin, CIT_COMMAND));
	assert_true(vle_mode_is(NORMAL_MODE));
}

TEST(script_failure_is_handled)
{
	update_string(&cfg.media_prg, "very/wrong/cmd/name");
	assert_failure(exec_commands("media", &lwin, CIT_COMMAND));
	assert_true(vle_mode_is(NORMAL_MODE));
}

TEST(menu_is_loaded)
{
	FILE *fp = fopen("script", "w");
	fputs("#!/bin/sh\n"
	      "echo device=/dev/sdf\n"
	      "echo label=sdf-label\n"
	      "echo mount-point=sdf-mount-point\n", fp);
	fclose(fp);

	assert_success(exec_commands("media", &lwin, CIT_COMMAND));

	assert_true(vle_mode_is(MENU_MODE));
	(void)vle_keys_exec(WK_ESC);
}

TEST(entries_are_formatted_correctly)
{
	FILE *fp = fopen("script", "w");
	fputs("#!/bin/sh\n"
	      "echo device=/dev/label-1mount\n"
	      "echo label=label\n"
	      "echo mount-point=mount-point\n"
	      "echo\n"
	      "echo device=/dev/nolabel-nomount\n"
	      ""
	      "echo device=/dev/label-2mounts\n"
	      "echo mount-point=mount-point1\n"
	      "echo mount-point=mount-point2\n"
	      "echo label=otherlabel\n", fp);
	fclose(fp);

	assert_success(exec_commands("media", &lwin, CIT_COMMAND));

	assert_int_equal(9, menu_get_current()->len);

	assert_string_equal("/dev/label-1mount [label]",
			menu_get_current()->items[0]);
	assert_string_equal("  -- mount-point", menu_get_current()->items[1]);
	assert_string_equal("", menu_get_current()->items[2]);
	assert_string_equal("/dev/nolabel-nomount", menu_get_current()->items[3]);
	assert_string_equal("  -- not mounted", menu_get_current()->items[4]);
	assert_string_equal("", menu_get_current()->items[5]);
	assert_string_equal("/dev/label-2mounts [otherlabel]",
			menu_get_current()->items[6]);
	assert_string_equal("  -- mount-point1", menu_get_current()->items[7]);
	assert_string_equal("  -- mount-point2", menu_get_current()->items[8]);
	assert_string_equal(NULL, menu_get_current()->data[0]);
	assert_string_equal("umount-point", menu_get_current()->data[1]);
	assert_string_equal(NULL, menu_get_current()->data[2]);
	assert_string_equal(NULL, menu_get_current()->data[3]);
	assert_string_equal("m/dev/nolabel-nomount", menu_get_current()->data[4]);
	assert_string_equal(NULL, menu_get_current()->data[5]);
	assert_string_equal(NULL, menu_get_current()->data[6]);
	assert_string_equal("umount-point1", menu_get_current()->data[7]);
	assert_string_equal("umount-point2", menu_get_current()->data[8]);
}

TEST(enter_does_nothing_on_non_mount_line)
{
	FILE *fp = fopen("script", "w");
	fputs("#!/bin/sh\n"
	      "echo device=/dev/sdf\n"
	      "echo label=sdf-label\n"
	      "echo mount-point=sdf-mount-point\n", fp);
	fclose(fp);

	assert_success(exec_commands("media", &lwin, CIT_COMMAND));

	lwin.curr_dir[0] = '\0';
	(void)vle_keys_exec(WK_CR);
	assert_string_equal("", lwin.curr_dir);
}

TEST(enter_navigates_to_mount_point)
{
	FILE *fp = fopen("script", "w");
	fprintf(fp, "#!/bin/sh\n"
	            "echo device=/dev/sdf\n"
	            "echo label=sdf-label\n"
	            "echo mount-point=%s\n", sandbox);
	fclose(fp);

	assert_success(exec_commands("media", &lwin, CIT_COMMAND));

	lwin.curr_dir[0] = '\0';
	(void)vle_keys_exec(WK_j);
	(void)vle_keys_exec(WK_CR);
	assert_true(paths_are_equal(lwin.curr_dir, sandbox));
}

TEST(unhandled_key_is_ignored)
{
	FILE *fp = fopen("script", "w");
	fprintf(fp, "#!/bin/sh\n"
	            "echo device=/dev/sdf\n"
	            "echo label=sdf-label\n"
	            "echo mount-point=%s\n", sandbox);
	fclose(fp);

	assert_success(exec_commands("media", &lwin, CIT_COMMAND));

	(void)vle_keys_exec(WK_x);
	(void)vle_keys_exec(WK_ESC);
}

TEST(r_reloads_list)
{
	FILE *fp = fopen("script", "w");
	fprintf(fp, "#!/bin/sh\n"
	            "echo device=/dev/sdf\n"
	            "echo label=sdf-label\n"
	            "echo mount-point=%s\n", sandbox);
	fclose(fp);

	assert_success(exec_commands("media", &lwin, CIT_COMMAND));

	fp = fopen("script", "w");
	fputs("#!/bin/sh\n"
	      "echo \"$@\" > out\n", fp);
	fclose(fp);

	(void)vle_keys_exec(WK_r);
	(void)vle_keys_exec(WK_ESC);

	int nlines;
	char **list = read_file_of_lines("out", &nlines);
	assert_int_equal(1, nlines);
	assert_string_equal("list", list[0]);
	free_string_array(list, nlines);

	assert_success(remove("out"));
}

TEST(m_toggles_mounts)
{
	FILE *fp = fopen("script", "w");
	fputs("#!/bin/sh\n"
	      "echo device=/dev/sdf\n"
	      "echo label=sdf-label\n"
	      "echo mount-point=first-mp\n", fp);
	fclose(fp);

	assert_success(exec_commands("media", &lwin, CIT_COMMAND));

	fp = fopen("script", "w");
	fputs("#!/bin/sh\n"
	      "echo device=/dev/sdf\n"
	      "echo \"$@\" >> out\n", fp);
	fclose(fp);

	/* Nothing happens for non-mountpoint line. */
	(void)vle_keys_exec(WK_m);
	assert_failure(remove("out"));

	(void)vle_keys_exec(WK_j);
	(void)vle_keys_exec(WK_m);
	(void)vle_keys_exec(WK_m);

	int nlines;
	char **list = read_file_of_lines("out", &nlines);
	assert_int_equal(4, nlines);
	assert_string_equal("unmount first-mp", list[0]);
	assert_string_equal("list", list[1]);
	assert_string_equal("mount /dev/sdf", list[2]);
	assert_string_equal("list", list[3]);
	free_string_array(list, nlines);

	(void)vle_keys_exec(WK_ESC);

	assert_success(remove("out"));
}

TEST(mounting_failure_is_handled)
{
	FILE *fp = fopen("script", "w");
	fputs("#!/bin/sh\n"
	      "echo device=/dev/sdf\n"
	      "echo label=sdf-label\n"
	      "echo mount-point=first-mp\n", fp);
	fclose(fp);

	assert_success(exec_commands("media", &lwin, CIT_COMMAND));

	fp = fopen("script", "w");
	fputs("#!/bin/sh\n"
	      "echo \"$@\" >> out\n"
	      "echo Things went bad 1>&2\n"
	      "exit 10\n", fp);
	fclose(fp);

	(void)vle_keys_exec(WK_j);
	(void)vle_keys_exec(WK_m);

	int nlines;
	char **list = read_file_of_lines("out", &nlines);
	assert_int_equal(1, nlines);
	assert_string_equal("unmount first-mp", list[0]);
	free_string_array(list, nlines);

	(void)vle_keys_exec(WK_ESC);

	assert_success(remove("out"));
}

TEST(mount_directory_is_left_before_unmounting)
{
	FILE *fp = fopen("script", "w");
	fprintf(fp, "#!/bin/sh\n"
	            "pwd >> %s/out\n"
	            "echo device=/dev/sdf\n"
	            "echo mount-point=%s\n", sandbox, sandbox);
	fclose(fp);

	free(cfg.media_prg);
	cfg.media_prg = format_str("%s/script", sandbox);

	strcpy(lwin.curr_dir, sandbox);
	assert_success(chdir(sandbox));
	assert_success(exec_commands("media", &lwin, CIT_COMMAND));

	(void)vle_keys_exec(WK_j);
	(void)vle_keys_exec(WK_m);

	char cwd[PATH_MAX + 1];
	assert_string_equal(cwd, get_cwd(cwd, sizeof(cwd)));
	assert_false(paths_are_same(cwd, sandbox));

	(void)vle_keys_exec(WK_ESC);
	assert_success(chdir(sandbox));

	int nlines;
	char **list = read_file_of_lines("out", &nlines);
	assert_int_equal(3, nlines);
	assert_string_equal(sandbox, list[0]);
	assert_false(strcmp(sandbox, list[1]) == 0);
	assert_false(strcmp(sandbox, list[2]) == 0);
	free_string_array(list, nlines);

	assert_success(remove("out"));
}

TEST(mount_matching_current_path_is_picked_by_default)
{
	FILE *fp = fopen("script", "w");
	fprintf(fp, "#!/bin/sh\n"
	            "echo device=/dev/sdd\n"
	            "echo mount-point=/bla\n"
	            "echo device=/dev/sdf\n"
	            "echo mount-point=%s\n", sandbox);
	fclose(fp);

	strcpy(lwin.curr_dir, sandbox);
	assert_success(exec_commands("media", &lwin, CIT_COMMAND));

	assert_int_equal(4, menu_get_current()->pos);

	(void)vle_keys_exec(WK_ESC);
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
