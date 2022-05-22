#include <stic.h>

#include <unistd.h> /* chdir() */

#include <stdio.h> /* fclose() fopen() remove() snprintf() */

#include "../../src/compat/fs_limits.h"
#include "../../src/utils/fs.h"
#include "../../src/trash.h"
#include "../../src/undo.h"

static OpsResult exec_func(OPS op, void *data, const char src[],
		const char dst[]);

static char trash_path[PATH_MAX + 1];
static char full_src[PATH_MAX + 1];
static char full_dst[PATH_MAX + 1];

SETUP()
{
	static int undo_levels = 3;
	un_init(&exec_func, NULL, NULL, &undo_levels);

	char *saved_cwd = save_cwd();
	assert_success(chdir(SANDBOX_PATH));
	assert_non_null(get_cwd(trash_path, sizeof(trash_path)));
	restore_cwd(saved_cwd);

	trash_set_specs(trash_path);

	snprintf(full_src, sizeof(full_src), "%s/src", trash_path);
	snprintf(full_dst, sizeof(full_dst), "%s/dst", trash_path);
}

TEARDOWN()
{
	un_reset();
}

TEST(conflicts_in_trash_are_automatically_resolved)
{
	fclose(fopen(SANDBOX_PATH "/dst", "w"));

	un_group_open("msg0");
	assert_success(un_group_add_op(OP_MOVE, NULL, NULL, full_src, full_dst));
	un_group_close();

	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	fclose(fopen(SANDBOX_PATH "/src", "w"));
	assert_int_equal(UN_ERR_SUCCESS, un_group_redo());

	assert_success(remove(SANDBOX_PATH "/src"));
	assert_success(remove(SANDBOX_PATH "/dst"));
}

TEST(clearing_all_commands_in_specific_trash)
{
	fclose(fopen(SANDBOX_PATH "/dst", "w"));

	un_group_open("msg0");
	assert_success(un_group_add_op(OP_MOVE, NULL, NULL, full_src, full_dst));
	un_group_close();

	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());

	assert_false(un_last_group_empty());
	un_clear_cmds_with_trash(trash_path);
	assert_true(un_last_group_empty());

	assert_success(remove(SANDBOX_PATH "/dst"));
}

TEST(clearing_all_commands_in_all_trashes)
{
	un_group_open("msg0");
	assert_success(un_group_add_op(OP_MOVE, NULL, NULL, full_src, full_dst));
	un_group_close();

	assert_false(un_last_group_empty());
	un_clear_cmds_with_trash(NULL);
	assert_true(un_last_group_empty());
}

static OpsResult
exec_func(OPS op, void *data, const char src[], const char dst[])
{
	return OPS_SUCCEEDED;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
