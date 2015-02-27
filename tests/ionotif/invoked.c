#include <stic.h>

#include <stddef.h> /* NULL */

#include "../../src/io/ioeta.h"
#include "../../src/io/ionotif.h"
#include "../../src/io/iop.h"
#include "../../src/io/ior.h"

static void progress_changed(const io_progress_t *const progress);

static int invoked_eta;
static int invoked_progress;

static ioeta_estim_t *estim;

SETUP()
{
	estim = ioeta_alloc(NULL);

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
progress_changed(const io_progress_t *const progress)
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
	ioeta_calculate(estim, "../read/binary-data", 0);

	{
		io_args_t args =
		{
			.arg1.src = "../read/binary-data",
			.arg2.dst = "binary-data",

			.estim = estim,
		};
		assert_int_equal(0, ior_cp(&args));
	}

	assert_int_equal(1, invoked_eta);
	assert_true(invoked_progress >= 1);
}

/* Relies on the previous test. */
TEST(mv_file_invokes_ionotif)
{
	ioeta_calculate(estim, "copy-of-binary-data", 0);

	{
		io_args_t args =
		{
			.arg1.src = "binary-data",
			.arg2.dst = "copy-of-binary-data",

			.estim = estim,
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_int_equal(1, invoked_eta);
	assert_true(invoked_progress >= 1);
}

/* Relies on the previous test. */
TEST(rm_file_invokes_ionotif)
{
	ioeta_calculate(estim, "copy-of-binary-data", 0);

	{
		io_args_t args =
		{
			.arg1.path = "copy-of-binary-data",

			.estim = estim,
		};
		assert_int_equal(0, ior_rm(&args));
	}

	assert_int_equal(1, invoked_eta);
	assert_true(invoked_progress >= 1);
}

TEST(cp_dir_invokes_ionotif)
{
	ioeta_calculate(estim, "../read", 0);

	{
		io_args_t args =
		{
			.arg1.src = "../read",
			.arg2.dst = "read",

			.estim = estim,
		};
		assert_int_equal(0, ior_cp(&args));
	}

	assert_int_equal(6, invoked_eta);
	assert_true(invoked_progress >= 1);
}

/* Relies on the previous test. */
TEST(mv_dir_invokes_ionotif)
{
	ioeta_calculate(estim, "moved-read", 0);

	{
		io_args_t args =
		{
			.arg1.src = "read",
			.arg2.dst = "moved-read",

			.estim = estim,
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_int_equal(1, invoked_eta);
	assert_true(invoked_progress >= 1);
}

/* Relies on the previous test. */
TEST(rm_dir_invokes_ionotif)
{
	ioeta_calculate(estim, "moved-read", 0);

	{
		io_args_t args =
		{
			.arg1.path = "moved-read",

			.estim = estim,
		};
		assert_int_equal(0, ior_rm(&args));
	}

	assert_int_equal(6, invoked_eta);
	assert_true(invoked_progress >= 1);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
