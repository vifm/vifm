#include <stic.h>

#include <sys/stat.h> /* chmod()  */
#include <unistd.h> /* rmdir() */

#include "../../src/compat/os.h"
#include "../../src/io/ioeta.h"
#include "../../src/io/iop.h"
#include "../../src/utils/utils.h"

#include "utils.h"

static IoErrCbResult handle_errors(struct io_args_t *args,
		const ioe_err_t *err);

static const io_cancellation_t no_cancellation;
static int retry_count;

TEST(file_removal_error_is_reported_and_logged_once)
{
	io_args_t args = {
		.arg1.path = SANDBOX_PATH "/file",

		.result.errors_cb = &handle_errors,
	};
	ioe_errlst_init(&args.result.errors);

	retry_count = 2;
	assert_failure(iop_rmfile(&args));
	assert_int_equal(0, retry_count);

	/* There are two retry requests, but only one error in the log. */
	assert_int_equal(1, args.result.errors.error_count);
	ioe_errlst_free(&args.result.errors);
}

TEST(dir_removal_error_is_reported_and_logged_once)
{
	io_args_t args = {
		.arg1.path = SANDBOX_PATH "/dir",

		.result.errors_cb = &handle_errors,
	};
	ioe_errlst_init(&args.result.errors);

	retry_count = 2;
	assert_failure(iop_rmdir(&args));
	assert_int_equal(0, retry_count);

	/* There are two retry requests, but only one error in the log. */
	assert_int_equal(1, args.result.errors.error_count);
	ioe_errlst_free(&args.result.errors);
}

TEST(retry_does_not_mess_up_estimations, IF(not_windows))
{
	io_args_t args = {
		.arg1.path = SANDBOX_PATH "/file",

		.estim = ioeta_alloc(NULL, no_cancellation),

		.result.errors = IOE_ERRLST_INIT,
		.result.errors_cb = &handle_errors,
	};

	assert_success(os_mkdir(SANDBOX_PATH "/dir", 0700));
	clone_test_file(TEST_DATA_PATH "/read/utf8-bom", SANDBOX_PATH "/dir/file");
	assert_success(chmod(SANDBOX_PATH "/dir", 0500));

	ioeta_calculate(args.estim, SANDBOX_PATH "/dir/file", 0);
	assert_int_equal(1, args.estim->total_items);
	assert_int_equal(0, args.estim->current_item);
	assert_int_equal(0, args.estim->inspected_items);
	assert_int_equal(0, args.estim->current_byte);
	assert_int_equal(9, args.estim->total_bytes);

	retry_count = 2;
	assert_failure(iop_rmfile(&args));

	assert_int_equal(1, args.estim->total_items);
	assert_int_equal(1, args.estim->current_item);
	assert_int_equal(1, args.estim->inspected_items);
	assert_int_equal(0, args.estim->current_byte);
	assert_int_equal(9, args.estim->total_bytes);

	ioe_errlst_free(&args.result.errors);
	ioeta_free(args.estim);

	assert_success(chmod(SANDBOX_PATH "/dir", 0700));
	delete_test_file(SANDBOX_PATH "/dir/file");
	assert_success(rmdir(SANDBOX_PATH "/dir"));
}

static IoErrCbResult
handle_errors(struct io_args_t *args, const ioe_err_t *err)
{
	if(retry_count > 0)
	{
		--retry_count;
		return IO_ECR_RETRY;
	}

	return IO_ECR_BREAK;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
