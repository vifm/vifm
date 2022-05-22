#include <stic.h>

#include <sys/types.h>
#include <unistd.h> /* F_OK access() geteuid() rmdir() */

#include <stdio.h> /* snprintf() */

#include "../../src/compat/fs_limits.h"
#include "../../src/io/ioeta.h"
#include "../../src/io/iop.h"
#include "../../src/io/ior.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"

#define DIR_NAME SANDBOX_PATH "/dir-to-create"
#define NESTED_DIR_NAME SANDBOX_PATH "/dir-to-create/dir-to-create"

static void create_directory(const char path[], const char root[],
		int create_parents);
static int non_root_on_unix_like_os(void);

TEST(single_dir_is_created)
{
	create_directory(DIR_NAME, DIR_NAME, 0);
}

TEST(child_dir_and_parent_are_created)
{
	create_directory(NESTED_DIR_NAME, DIR_NAME, 1);
}

TEST(create_by_absolute_path)
{
	if(!is_path_absolute(NESTED_DIR_NAME))
	{
		char cwd[PATH_MAX + 1];
		char full_path[PATH_MAX + 1];

		get_cwd(cwd, sizeof(cwd));
		snprintf(full_path, sizeof(full_path), "%s/%s", cwd, NESTED_DIR_NAME);
		create_directory(full_path, DIR_NAME, 1);
	}
}

static void
create_directory(const char path[], const char root[], int create_parents)
{
	assert_failure(access(path, F_OK));

	{
		io_args_t args = {
			.arg1.path = path,
			.arg2.process_parents = create_parents,
			.arg3.mode = 0700,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_mkdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(access(path, F_OK));
	assert_true(is_dir(path));

	{
		io_args_t args = {
			.arg1.path = root,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(child_dir_is_not_created)
{
	assert_failure(access(NESTED_DIR_NAME, F_OK));

	{
		io_args_t args = {
			.arg1.path = NESTED_DIR_NAME,
			.arg2.process_parents = 0,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_FAILED, iop_mkdir(&args));

		assert_true(args.result.errors.error_count != 0);
		ioe_errlst_free(&args.result.errors);
	}

	assert_failure(access(NESTED_DIR_NAME, F_OK));
}

TEST(creating_dirs_reports_progress)
{
	const io_cancellation_t no_cancellation = {};

	io_args_t args = {
		.arg1.path = DIR_NAME,

		.estim = ioeta_alloc(NULL, no_cancellation),
	};
	ioe_errlst_init(&args.result.errors);

	assert_int_equal(IO_RES_SUCCEEDED, iop_mkdir(&args));
	assert_int_equal(0, args.result.errors.error_count);

	assert_success(rmdir(DIR_NAME));
	assert_int_equal(1, args.estim->current_item);
	assert_true(args.estim->item != NULL);

	ioeta_free(args.estim);
}

TEST(permissions_are_taken_into_account, IF(non_root_on_unix_like_os))
{
	{
		io_args_t args = {
			.arg1.path = DIR_NAME,
			.arg2.process_parents = 0,
			.arg3.mode = 0000,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_mkdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = NESTED_DIR_NAME,
			.arg2.process_parents = 0,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_FAILED, iop_mkdir(&args));

		assert_true(args.result.errors.error_count != 0);
		ioe_errlst_free(&args.result.errors);
	}

	{
		io_args_t args = {
			.arg1.path = DIR_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_rmdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(permissions_are_taken_into_account_for_the_most_nested_only,
     IF(non_root_on_unix_like_os))
{
	{
		io_args_t args = {
			.arg1.path = NESTED_DIR_NAME,
			.arg2.process_parents = 1,
			.arg3.mode = 0000,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_mkdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = DIR_NAME "/dir",
			.arg2.process_parents = 0,
			.arg3.mode = 0755,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_mkdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = NESTED_DIR_NAME "/dir",
			.arg2.process_parents = 0,
			.arg3.mode = 0755,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_FAILED, iop_mkdir(&args));

		assert_true(args.result.errors.error_count != 0);
		ioe_errlst_free(&args.result.errors);
	}

	{
		io_args_t args = {
			.arg1.path = NESTED_DIR_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_rmdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = DIR_NAME "/dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_rmdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = DIR_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_rmdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

/* Permissions don't work on non-*nix-like systems and some checks fail for
 * root user because system allows him to do almost anything and tests are
 * counting on permissions denial. */
static int
non_root_on_unix_like_os(void)
{
#if defined(_WIN32) || defined(__CYGWIN__)
	return 0;
#else
	return (geteuid() != 0);
#endif
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
