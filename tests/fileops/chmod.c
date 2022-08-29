#include <stic.h>

#include <sys/stat.h> /* chmod() */
#include <unistd.h> /* chdir() rmdir() */

#include <stdlib.h> /* free() */
#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/compat/os.h"
#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/str.h"
#include "../../src/modes/dialogs/attr_dialog.h"
#include "../../src/filelist.h"

#ifndef _WIN32

static void set_file_perms(const int perms[13]);
static void alloc_file_list(view_t *view, const char filename[]);
static mode_t perms_to_mode(const int perms[13]);
static mode_t get_perms(const char path[]);
static int can_reset_x_on_files(void);

static mode_t mask;

SETUP_ONCE()
{
	replace_string(&cfg.shell, "/bin/sh");
	replace_string(&cfg.shell_cmd_flag, "-c");
	stats_update_shell_type(cfg.shell);
}

SETUP()
{
	mask = umask(0000);
	view_setup(&lwin);
}

TEARDOWN()
{
	(void)umask(mask);
	view_teardown(&lwin);
}

TEST(every_permission_can_be_reset)
{
	int idxs[] = { 0, 1, 2,  4, 5, 6,  7, 8, 9 };
	int perms[13] = { [0]  = 1, [1] = 1, [2]  = 1, [3]  = 0,
	                  [4]  = 1, [5] = 1, [6]  = 1, [7]  = 0,
	                  [8]  = 1, [9] = 1, [10] = 1, [11] = 0,
	                  [12] = 0 };
	unsigned int i;
	for(i = 0U; i < ARRAY_LEN(idxs); ++i)
	{
		perms[i] ^= 1;
		set_file_perms(perms);
		perms[i] ^= 1;

		view_teardown(&lwin);
		view_setup(&lwin);
	}
}

static void
set_file_perms(const int perms[13])
{
	FILE *f;

	int origin_perms[13] = { 1, 1, 1, 0,  1, 1, 1, 0,  1, 1, 1, 0,  0 };
	int adv_perms[3]     = { 0, 0, 0 };

	assert_non_null(f = fopen(SANDBOX_PATH "/file", "w"));
	fclose(f);
	assert_success(chmod(SANDBOX_PATH "/file", 0777));

	if(get_perms(SANDBOX_PATH "/file") != 0777)
	{
		assert_success(unlink(SANDBOX_PATH "/file"));
		return;
	}

	strcpy(lwin.curr_dir, SANDBOX_PATH);
	alloc_file_list(&lwin, "file");
	flist_set_marking(&lwin, 0);
	set_perm_string(&lwin, perms, origin_perms, adv_perms);

	assert_int_equal(perms_to_mode(perms), get_perms(SANDBOX_PATH "/file"));

	assert_success(unlink(SANDBOX_PATH "/file"));
}

TEST(reset_executable_bits_from_files_only, IF(can_reset_x_on_files))
{
	FILE *f;

	int perms[13]        = { 1, 1, 0, 0,  1, 1, 0, 0,  1, 1, 0, 0,  1 };
	int adv_perms[3]     = { 1, 1, 1 };
	int origin_perms[13] = { 1, 1, 1, 0,  1, 1, 1, 0,  1, 1, 1, 0,  1 };

	assert_success(os_mkdir(SANDBOX_PATH "/dir", 0777));

	assert_non_null(f = fopen(SANDBOX_PATH "/dir/file", "w"));
	fclose(f);
	assert_success(chmod(SANDBOX_PATH "/dir/file", 0777));

	if(get_perms(SANDBOX_PATH "/dir") != 0777 ||
			get_perms(SANDBOX_PATH "/dir/file") != 0777)
	{
		assert_success(unlink(SANDBOX_PATH "/dir/file"));
		assert_success(rmdir(SANDBOX_PATH "/dir"));
		return;
	}

	strcpy(lwin.curr_dir, SANDBOX_PATH);
	alloc_file_list(&lwin, "dir");
	flist_set_marking(&lwin, 0);
	set_perm_string(&lwin, perms, origin_perms, adv_perms);

	assert_int_equal(perms_to_mode(perms), get_perms(SANDBOX_PATH "/dir/file"));
	assert_int_equal(0777, get_perms(SANDBOX_PATH "/dir"));

	assert_success(unlink(SANDBOX_PATH "/dir/file"));
	assert_success(rmdir(SANDBOX_PATH "/dir"));
}

TEST(set_executable_bit_via_X_flag)
{
	FILE *f;

	int perms[13]        = { 1, 1, 1, 0,  1, 1, 1, 0,  1, 1, 0, 0,  1 };
	int adv_perms[3]     = { 1, 0, 0 };
	int origin_perms[13] = { 1, 1, 0, 0,  1, 1, 1, 0,  1, 1, 0, 0,  1 };

	assert_success(os_mkdir(SANDBOX_PATH "/dir", 0777));

	assert_non_null(f = fopen(SANDBOX_PATH "/dir/file", "w"));
	fclose(f);
	assert_success(chmod(SANDBOX_PATH "/dir/file", 0676));

	if(get_perms(SANDBOX_PATH "/dir") != 0777 ||
			get_perms(SANDBOX_PATH "/dir/file") != 0676)
	{
		assert_success(unlink(SANDBOX_PATH "/dir/file"));
		assert_success(rmdir(SANDBOX_PATH "/dir"));
		return;
	}

	strcpy(lwin.curr_dir, SANDBOX_PATH);
	alloc_file_list(&lwin, "dir");
	flist_set_marking(&lwin, 0);
	set_perm_string(&lwin, perms, origin_perms, adv_perms);

	assert_int_equal(0776, get_perms(SANDBOX_PATH "/dir/file"));
	assert_int_equal(0776, get_perms(SANDBOX_PATH "/dir"));

	assert_success(unlink(SANDBOX_PATH "/dir/file"));
	assert_success(rmdir(SANDBOX_PATH "/dir"));
}

static void
alloc_file_list(view_t *view, const char filename[])
{
	view->list_rows = 1;
	view->list_pos = 0;
	view->dir_entry = dynarray_cextend(NULL,
			view->list_rows*sizeof(*view->dir_entry));
	view->dir_entry[0].name = strdup(filename);
	view->dir_entry[0].origin = &view->curr_dir[0];
}

static mode_t
perms_to_mode(const int perms[13])
{
	return (perms[0] << 8) | (perms[1] << 7) | (perms[2] << 6)
	     | (perms[4] << 5)  | (perms[5] << 4)  | (perms[6] << 3)
	     | (perms[8] << 2)  | (perms[9] << 1)  | (perms[10] << 0);
}

static mode_t
get_perms(const char path[])
{
	struct stat st;
	assert_success(stat(path, &st));
	return (st.st_mode & 0777);
}

/* BSD-like chmod uses original permissions when evaluating X modifier. */
static int
can_reset_x_on_files(void)
{
	FILE *f;
	assert_non_null(f = fopen(SANDBOX_PATH "/file", "w"));
	fclose(f);

	assert_success(chmod(SANDBOX_PATH "/file", 0777));
	assert_int_equal(0777, get_perms(SANDBOX_PATH "/file"));

	assert_success(os_system("chmod a-x+X " SANDBOX_PATH "/file"));
	int perms = get_perms(SANDBOX_PATH "/file");
	assert_success(unlink(SANDBOX_PATH "/file"));

	return (perms == 0666);
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
