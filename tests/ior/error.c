#include <stic.h>

#include <sys/stat.h> /* chmod()  */
#include <unistd.h> /* rmdir() */

#include "../../src/compat/os.h"
#include "../../src/io/ior.h"

#include "utils.h"

static IoErrCbResult handle_errors(struct io_args_t *args,
		const ioe_err_t *err);

static int ignore_count;

TEST(file_removal_error_is_reported_and_logged_once, IF(not_windows))
{
	os_mkdir(SANDBOX_PATH "/dir", 0700);
	create_empty_file(SANDBOX_PATH "/dir/file");
	assert_success(chmod(SANDBOX_PATH "/dir", 0500));

	{
		io_args_t args = {
			.arg1.path = TEST_DATA_PATH "/dir",

			.result.errors_cb = &handle_errors,
		};
		ioe_errlst_init(&args.result.errors);

		ignore_count = 0;
		assert_failure(ior_rm(&args));
		assert_int_equal(0, ignore_count);

		/* Second error must be about failure to remove directory, first one is
		 * ignored and doesn't make it into the list. */
		assert_int_equal(1, args.result.errors.error_count);
		ioe_errlst_free(&args.result.errors);
	}

	assert_success(chmod(SANDBOX_PATH "/dir", 0700));
	delete_file(SANDBOX_PATH "/dir/file");
	rmdir(SANDBOX_PATH "/dir");
}

static IoErrCbResult
handle_errors(struct io_args_t *args, const ioe_err_t *err)
{
	if(ignore_count > 0)
	{
		--ignore_count;
		return IO_ECR_IGNORE;
	}

	return IO_ECR_BREAK;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
