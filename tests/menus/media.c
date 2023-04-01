#ifndef _WIN32

#include <stic.h>

#include <unistd.h> /* chdir() */

#include <stddef.h> /* NULL */
#include <stdio.h> /* remove() */
#include <string.h> /* strcmp() strcpy() */

#include <test-utils.h>

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

static char sandbox[PATH_MAX + 1];
static char *saved_cwd;

#define SHEBANG_ECHO "#!/usr/bin/tail -n+2\n"

SETUP_ONCE()
{
	char cwd[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
	make_abs_path(sandbox, sizeof(sandbox), SANDBOX_PATH, "", cwd);
}

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;
	view_setup(&lwin);

	/* The redraw code updates 'columns' and 'lines'. */
	opt_handlers_setup();
	modes_init();
	cmds_init();

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
	opt_handlers_teardown();

	view_teardown(&lwin);
	curr_view = NULL;
	other_view = NULL;

	curr_stats.load_stage = 0;
}

TEST(menu_not_created_if_no_devices)
{
	FILE *fp = fopen("script", "w");
	fputs(SHEBANG_ECHO
	      "mount-point=no-device\n"
	      "nothing", fp);
	fclose(fp);

	assert_failure(cmds_dispatch("media", &lwin, CIT_COMMAND));
	assert_true(vle_mode_is(NORMAL_MODE));
}

TEST(menu_aborts_if_mediaprg_is_not_set)
{
	update_string(&cfg.media_prg, "");

	assert_success(cmds_dispatch("media", &lwin, CIT_COMMAND));
	assert_true(vle_mode_is(NORMAL_MODE));
}

TEST(script_failure_is_handled)
{
	update_string(&cfg.media_prg, "very/wrong/cmd/name");
	assert_failure(cmds_dispatch("media", &lwin, CIT_COMMAND));
	assert_true(vle_mode_is(NORMAL_MODE));
}

TEST(menu_is_loaded)
{
	FILE *fp = fopen("script", "w");
	fputs(SHEBANG_ECHO
	      "device=/dev/sdf", fp);
	fclose(fp);

	assert_success(cmds_dispatch("media", &lwin, CIT_COMMAND));

	assert_true(vle_mode_is(MENU_MODE));
	(void)vle_keys_exec(WK_ESC);
}

TEST(entries_are_formatted_correctly)
{
	FILE *fp = fopen("script", "w");
	fputs(SHEBANG_ECHO
	      "device=/dev/label-1mount\n"
	      "label=ignored label\n"
	      "label=label\n"
	      "garbage lines\n\n\n\n"
	      "mount-point=mount-point\n"
	      "device=/dev/nolabel-nomount\n"
	      "\n"
	      "device=/dev/label-2mounts\n"
	      "mount-point=mount-point1\n"
	      "mount-point=mount-point2\n"
	      "mount-point=mount-point3\n"
	      "info=device info [my label]\n"
	      "label=ignored label\n", fp);
	fclose(fp);

	assert_success(cmds_dispatch("media", &lwin, CIT_COMMAND));

	assert_int_equal(10, menu_get_current()->len);

	assert_string_equal("/dev/label-1mount [label]",
			menu_get_current()->items[0]);
	assert_string_equal("`-- mount-point", menu_get_current()->items[1]);
	assert_string_equal("", menu_get_current()->items[2]);
	assert_string_equal("/dev/nolabel-nomount ", menu_get_current()->items[3]);
	assert_string_equal("`-- (not mounted)", menu_get_current()->items[4]);
	assert_string_equal("", menu_get_current()->items[5]);
	assert_string_equal("/dev/label-2mounts device info [my label]",
			menu_get_current()->items[6]);
	assert_string_equal("|-- mount-point1", menu_get_current()->items[7]);
	assert_string_equal("|-- mount-point2", menu_get_current()->items[8]);
	assert_string_equal("`-- mount-point3", menu_get_current()->items[9]);
	assert_string_equal(NULL, menu_get_current()->data[0]);
	assert_string_equal("umount-point", menu_get_current()->data[1]);
	assert_string_equal(NULL, menu_get_current()->data[2]);
	assert_string_equal(NULL, menu_get_current()->data[3]);
	assert_string_equal("m/dev/nolabel-nomount", menu_get_current()->data[4]);
	assert_string_equal(NULL, menu_get_current()->data[5]);
	assert_string_equal(NULL, menu_get_current()->data[6]);
	assert_string_equal("umount-point1", menu_get_current()->data[7]);
	assert_string_equal("umount-point2", menu_get_current()->data[8]);
	assert_string_equal("umount-point3", menu_get_current()->data[9]);

	(void)vle_keys_exec(WK_ESC);
}

TEST(enter_navigates_to_mount_point)
{
	FILE *fp = fopen("script", "w");
	fprintf(fp, SHEBANG_ECHO
	            "device=/dev/sdf\n"
	            "mount-point=%s\n", sandbox);
	fclose(fp);

	assert_success(cmds_dispatch("media", &lwin, CIT_COMMAND));

	lwin.curr_dir[0] = '\0';
	(void)vle_keys_exec(WK_j);
	(void)vle_keys_exec(WK_CR);
	assert_true(paths_are_equal(lwin.curr_dir, sandbox));
}

TEST(enter_navigates_to_mount_point_on_device_line)
{
	FILE *fp = fopen("script", "w");
	fprintf(fp, SHEBANG_ECHO
	            "device=/dev/sdf\n"
	            "mount-point=%s\n", sandbox);
	fclose(fp);

	assert_success(cmds_dispatch("media", &lwin, CIT_COMMAND));

	lwin.curr_dir[0] = '\0';
	(void)vle_keys_exec(WK_CR);
	assert_true(paths_are_equal(lwin.curr_dir, sandbox));
}

TEST(enter_mounts_unmounted_device)
{
	FILE *fp = fopen("script", "w");
	fputs("#!/bin/sh\n"
	      "if [ \"$1\" = list ]; then\n"
	      "  echo device=/dev/sdf1\n"
	      "fi\n"
	      "echo \"$@\" >> out\n", fp);
	fclose(fp);

	assert_success(cmds_dispatch("media", &lwin, CIT_COMMAND));

	(void)remove("out");

	/* Mounts on device line. */
	(void)vle_keys_exec(WK_CR);

	/* Mounts on "not mounted" line. */
	(void)vle_keys_exec(WK_j);
	(void)vle_keys_exec(WK_CR);

	int nlines;
	char **list = read_file_of_lines("out", &nlines);
	assert_int_equal(4, nlines);
	assert_string_equal("mount /dev/sdf1", list[0]);
	assert_string_equal("list",            list[1]);
	assert_string_equal("mount /dev/sdf1", list[2]);
	assert_string_equal("list",            list[3]);

	free_string_array(list, nlines);

	(void)vle_keys_exec(WK_ESC);

	assert_success(remove("out"));
}

TEST(enter_does_nothing_on_device_lines_with_multiple_mounts)
{
	FILE *fp = fopen("script", "w");
	fputs(SHEBANG_ECHO
	      "device=/dev/sdf\n"
	      "label=sdf-label\n"
	      "mount-point=mount-point1\n"
	      "mount-point=mount-point2\n", fp);
	fclose(fp);

	assert_success(cmds_dispatch("media", &lwin, CIT_COMMAND));

	lwin.curr_dir[0] = '\0';
	(void)vle_keys_exec(WK_CR);
	assert_string_equal("", lwin.curr_dir);
}

TEST(unhandled_key_is_ignored)
{
	FILE *fp = fopen("script", "w");
	fprintf(fp, SHEBANG_ECHO
	            "device=/dev/sdf\n"
	            "label=sdf-label\n"
	            "mount-point=%s\n", sandbox);
	fclose(fp);

	assert_success(cmds_dispatch("media", &lwin, CIT_COMMAND));

	(void)vle_keys_exec(WK_x);
	(void)vle_keys_exec(WK_ESC);
}

TEST(r_reloads_list)
{
	FILE *fp = fopen("script", "w");
	fprintf(fp, SHEBANG_ECHO
	            "device=/dev/sdf\n"
	            "label=sdf-label\n"
	            "mount-point=%s\n", sandbox);
	fclose(fp);

	assert_success(cmds_dispatch("media", &lwin, CIT_COMMAND));

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
	      "if [ \"$1\" = list ]; then\n"
	      "cat <<EOF\n"
	      "device=/dev/sdf1\n"
	      "\n"
	      "device=/dev/sdf2\n"
	      "mount-point=sdf2-mp1\n"
	      "\n"
	      "device=/dev/sdf3\n"
	      "mount-point=sdf3-mp1\n"
	      "mount-point=sdf3-mp2\n"
	      "EOF\n"
	      "fi\n"
	      "echo \"$@\" >> out\n", fp);
	fclose(fp);

	assert_success(cmds_dispatch("media", &lwin, CIT_COMMAND));

	(void)remove("out");

	/* Mounts on device line. */
	(void)vle_keys_exec(WK_m);

	/* Mounts on "not mounted" line. */
	(void)vle_keys_exec(WK_j);
	(void)vle_keys_exec(WK_m);

	/* Does nothing on blank lines. */
	(void)vle_keys_exec(WK_j);
	(void)vle_keys_exec(WK_m);

	/* Unmounts on device line with single mount-point. */
	(void)vle_keys_exec(WK_j);
	(void)vle_keys_exec(WK_m);

	/* Unmounts on (single) mount-point line. */
	(void)vle_keys_exec(WK_j);
	(void)vle_keys_exec(WK_m);

	/* Does nothing on blank lines. */
	(void)vle_keys_exec(WK_j);
	(void)vle_keys_exec(WK_m);

	/* Does not unmount on device line with multiple mount-points. */
	(void)vle_keys_exec(WK_j);
	(void)vle_keys_exec(WK_m);

	/* Unmounts on mount-point lines. */
	(void)vle_keys_exec(WK_j);
	(void)vle_keys_exec(WK_m);
	(void)vle_keys_exec(WK_j);
	(void)vle_keys_exec(WK_m);

	int nlines;
	char **list = read_file_of_lines("out", &nlines);
	assert_int_equal(12, nlines);
	assert_string_equal("mount /dev/sdf1",  list[0]);
	assert_string_equal("list",             list[1]);
	assert_string_equal("mount /dev/sdf1",  list[2]);
	assert_string_equal("list",             list[3]);

	assert_string_equal("unmount sdf2-mp1", list[4]);
	assert_string_equal("list",             list[5]);
	assert_string_equal("unmount sdf2-mp1", list[6]);
	assert_string_equal("list",             list[7]);

	assert_string_equal("unmount sdf3-mp1", list[8]);
	assert_string_equal("list",             list[9]);
	assert_string_equal("unmount sdf3-mp2", list[10]);
	assert_string_equal("list",             list[11]);

	free_string_array(list, nlines);

	(void)vle_keys_exec(WK_ESC);

	assert_success(remove("out"));
}

TEST(mounting_failure_is_handled)
{
	FILE *fp = fopen("script", "w");
	fputs(SHEBANG_ECHO
	      "device=/dev/sdf\n"
	      "mount-point=mount-point1\n", fp);
	fclose(fp);

	assert_success(cmds_dispatch("media", &lwin, CIT_COMMAND));

	fp = fopen("script", "w");
	fputs("#!/bin/sh\n"
	      "echo \"$@\" >> out\n"
	      "exit 10\n", fp);
	fclose(fp);

	(void)vle_keys_exec(WK_j);
	(void)vle_keys_exec(WK_m);

	int nlines;
	char **list = read_file_of_lines("out", &nlines);
	assert_int_equal(1, nlines);
	assert_string_equal("unmount mount-point1", list[0]);
	free_string_array(list, nlines);

	(void)vle_keys_exec(WK_ESC);

	assert_success(remove("out"));
}

TEST(mount_directory_is_left_before_unmounting)
{
	FILE *fp = fopen("script", "w");
	fprintf(fp, "#!/bin/sh\n"
	            "pwd >> %s/out\n"
	            "if [ \"$1\" = list ]; then\n"
	            "  echo device=/dev/sdf\n"
	            "  echo mount-point=%s\n"
	            "fi", sandbox, sandbox);
	fclose(fp);

	free(cfg.media_prg);
	cfg.media_prg = format_str("%s/script", sandbox);

	strcpy(lwin.curr_dir, sandbox);
	assert_success(chdir(sandbox));
	assert_success(cmds_dispatch("media", &lwin, CIT_COMMAND));

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
	fprintf(fp, SHEBANG_ECHO
	            "device=/dev/sdd\n"
	            "mount-point=/bla\n"
	            "device=/dev/sdf\n"
	            "mount-point=%s\n", sandbox);
	fclose(fp);

	strcpy(lwin.curr_dir, sandbox);
	assert_success(cmds_dispatch("media", &lwin, CIT_COMMAND));

	assert_int_equal(4, menu_get_current()->pos);

	(void)vle_keys_exec(WK_ESC);
}

TEST(barckets_navigates_between_devices)
{
	FILE *fp = fopen("script", "w");
	fputs(SHEBANG_ECHO
	      "device=dev\n"
	      ""
	      "\n"
	      "device=dev\n"
	      "mount-point=/\n"
	      "mount-point=/\n"
	      "mount-point=/\n"
	      "\n"
	      "device=dev\n"
	      "mount-point=/", fp);
	fclose(fp);

	strcpy(lwin.curr_dir, sandbox);
	assert_success(cmds_dispatch("media", &lwin, CIT_COMMAND));

	assert_int_equal(10, menu_get_current()->len);

	const int jumps_LB[] = { 0, 0, 0, 0, 3, 3, 3, 3, 3, 8 };
	const int jumps_RB[] = { 3, 3, 3, 8, 8, 8, 8, 8, 8, 9 };

#define key_exec_on_line(key, linepos) \
	menu_get_current()->pos = linepos; \
	(void)vle_keys_exec(WK_##key); \
	assert_int_equal(jumps_##key[linepos], menu_get_current()->pos);

	/* "["-jumps. */
	key_exec_on_line(LB, 0);
	key_exec_on_line(LB, 1);
	key_exec_on_line(LB, 2);
	key_exec_on_line(LB, 3);
	key_exec_on_line(LB, 4);
	key_exec_on_line(LB, 5);
	key_exec_on_line(LB, 6);
	key_exec_on_line(LB, 7);
	key_exec_on_line(LB, 8);
	key_exec_on_line(LB, 9);

	/* "]"-jumps. */
	key_exec_on_line(RB, 0);
	key_exec_on_line(RB, 1);
	key_exec_on_line(RB, 2);
	key_exec_on_line(RB, 3);
	key_exec_on_line(RB, 4);
	key_exec_on_line(RB, 5);
	key_exec_on_line(RB, 6);
	key_exec_on_line(RB, 7);
	key_exec_on_line(RB, 8);
	key_exec_on_line(RB, 9);

#undef key_exec_on_line

	(void)vle_keys_exec(WK_ESC);
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
