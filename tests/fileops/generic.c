#include <stic.h>

#include <unistd.h> /* unlink() */

#include <stddef.h> /* NULL */
#include <stdlib.h> /* calloc() free() */
#include <string.h> /* strcpy() strdup() */

#include "../../src/cfg/config.h"
#include "../../src/compat/os.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/macros.h"
#include "../../src/utils/str.h"
#include "../../src/fileops.h"
#include "../../src/ops.h"
#include "../../src/undo.h"

static void create_empty_dir(const char dir[]);
static void create_empty_file(const char file[]);
static int file_exists(const char file[]);

SETUP()
{
	assert_int_equal(0, chdir(SANDBOX_PATH));

	/* lwin */
	strcpy(lwin.curr_dir, ".");

	lwin.list_rows = 1;
	lwin.list_pos = 0;
	lwin.dir_entry = calloc(lwin.list_rows, sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("file");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];

	/* rwin */
	strcpy(rwin.curr_dir, ".");

	curr_view = &lwin;
	other_view = &rwin;
}

TEARDOWN()
{
	int i;

	for(i = 0; i < lwin.list_rows; ++i)
	{
		free(lwin.dir_entry[i].name);
	}
	free(lwin.dir_entry);

	assert_int_equal(0, chdir("../.."));
}

TEST(move_file)
{
	char new_fname[] = "new_name";
	char *list[] = { &new_fname[0] };

	FILE *const f = fopen(lwin.dir_entry[0].name, "w");
	fclose(f);

	assert_true(path_exists(lwin.dir_entry[0].name, DEREF));

	lwin.dir_entry[0].marked = 1;
	(void)cpmv_files(&lwin, list, ARRAY_LEN(list), CMLO_MOVE, 0);

	assert_false(path_exists(lwin.dir_entry[0].name, DEREF));
	assert_true(path_exists(new_fname, DEREF));

	(void)unlink(new_fname);
}

TEST(merge_directories)
{
#ifndef _WIN32
	replace_string(&cfg.shell, "/bin/sh");
#else
	replace_string(&cfg.shell, "cmd");
#endif

	stats_update_shell_type(cfg.shell);

	for(cfg.use_system_calls = 0; cfg.use_system_calls < 2;
			++cfg.use_system_calls)
	{
		ops_t *ops;

		create_empty_dir("first");
		create_empty_dir("first/nested");
		create_empty_file("first/nested/first-file");

		create_empty_dir("second");
		create_empty_dir("second/nested");
		create_empty_file("second/nested/second-file");

		cmd_group_begin("undo msg");

		assert_non_null(ops = ops_alloc(OP_MOVEF, "merge", ".", "."));
		ops->crp = CRP_OVERWRITE_ALL;
		assert_success(merge_dirs("first", "second", ops));
		ops_free(ops);

		cmd_group_end();

		/* Original directory must be deleted. */
		assert_false(file_exists("first/nested"));
		assert_false(file_exists("first"));

		assert_true(file_exists("second/nested/second-file"));
		assert_true(file_exists("second/nested/first-file"));

		assert_success(unlink("second/nested/first-file"));
		assert_success(unlink("second/nested/second-file"));
		assert_success(rmdir("second/nested"));
		assert_success(rmdir("second"));
	}

	stats_update_shell_type("/bin/sh");
}

static void
create_empty_dir(const char dir[])
{
	os_mkdir(dir, 0700);
	assert_true(is_dir(dir));
}

static void
create_empty_file(const char file[])
{
	FILE *const f = fopen(file, "w");
	fclose(f);
	assert_success(access(file, F_OK));
}

static int
file_exists(const char file[])
{
	return access(file, F_OK) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
