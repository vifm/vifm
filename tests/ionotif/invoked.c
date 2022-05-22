#include <stic.h>

#include <sys/stat.h> /* chmod() */

#include <stddef.h> /* NULL */

#include "../../src/io/ioeta.h"
#include "../../src/io/ionotif.h"
#include "../../src/io/iop.h"
#include "../../src/io/ior.h"

static void progress_changed(const io_progress_t *progress);

static int invoked_eta;
static int invoked_progress;

static ioeta_estim_t *estim;

SETUP()
{
	const io_cancellation_t no_cancellation = {};
	estim = ioeta_alloc(NULL, no_cancellation);

	invoked_eta = 0;
	invoked_progress = 0;

	ionotif_register(&progress_changed);
}

TEARDOWN()
{
	ionotif_register(NULL);

	ioeta_free(estim);
}

static void
progress_changed(const io_progress_t *progress)
{
	switch(progress->stage)
	{
		case IO_PS_ESTIMATING:
			++invoked_eta;
			break;
		case IO_PS_IN_PROGRESS:
			++invoked_progress;
			break;
	}
}

TEST(cp_file_invokes_ionotif)
{
	ioeta_calculate(estim, TEST_DATA_PATH "/read/binary-data", 0);

	{
		io_args_t args = {
			.arg1.src = TEST_DATA_PATH "/read/binary-data",
			.arg2.dst = SANDBOX_PATH "/binary-data",

			.estim = estim,
		};
		assert_int_equal(IO_RES_SUCCEEDED, ior_cp(&args));
	}

	assert_int_equal(1, invoked_eta);
	assert_true(invoked_progress >= 1);
}

/* Relies on the previous test. */
TEST(mv_file_invokes_ionotif)
{
	ioeta_calculate(estim, SANDBOX_PATH "/copy-of-binary-data", 0);

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/binary-data",
			.arg2.dst = SANDBOX_PATH "/copy-of-binary-data",

			.estim = estim,
		};
		assert_int_equal(IO_RES_SUCCEEDED, ior_mv(&args));
	}

	assert_int_equal(1, invoked_eta);
	assert_true(invoked_progress >= 1);
}

/* Relies on the previous test. */
TEST(rm_file_invokes_ionotif)
{
	ioeta_calculate(estim, SANDBOX_PATH "/copy-of-binary-data", 0);

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/copy-of-binary-data",

			.estim = estim,
		};
		assert_int_equal(IO_RES_SUCCEEDED, ior_rm(&args));
	}

	assert_int_equal(1, invoked_eta);
	assert_true(invoked_progress >= 1);
}

TEST(cp_dir_invokes_ionotif)
{
	ioeta_calculate(estim, TEST_DATA_PATH "/read", 0);

	{
		io_args_t args = {
			.arg1.src = TEST_DATA_PATH "/read",
			.arg2.dst = SANDBOX_PATH "/read",

			.estim = estim,
		};
		assert_int_equal(IO_RES_SUCCEEDED, ior_cp(&args));
	}

	assert_int_equal(7, invoked_eta);
	assert_true(invoked_progress >= 1);
}

/* Relies on the previous test. */
TEST(mv_dir_invokes_ionotif)
{
	ioeta_calculate(estim, SANDBOX_PATH "/moved-read", 0);

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/read",
			.arg2.dst = SANDBOX_PATH "/moved-read",

			.estim = estim,
		};
		assert_int_equal(IO_RES_SUCCEEDED, ior_mv(&args));
	}

	assert_int_equal(1, invoked_eta);
	assert_true(invoked_progress >= 1);
}

/* Relies on the previous test. */
TEST(rm_dir_invokes_ionotif)
{
	ioeta_calculate(estim, SANDBOX_PATH "/moved-read", 0);

	assert_success(chmod(SANDBOX_PATH "/moved-read", 0700));

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/moved-read",

			.estim = estim,
		};
		assert_int_equal(IO_RES_SUCCEEDED, ior_rm(&args));
	}

	assert_int_equal(7, invoked_eta);
	assert_true(invoked_progress >= 1);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
