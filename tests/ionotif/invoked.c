#include "seatest.h"

#include <stddef.h> /* NULL */

#include "../../src/io/ioeta.h"
#include "../../src/io/ionotif.h"
#include "../../src/io/iop.h"
#include "../../src/io/ior.h"

static int invoked_eta;
static int invoked_progress;

static ioeta_estim_t *estim;

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

static void
setup(void)
{
	estim = ioeta_alloc(NULL);

	invoked_eta = 0;
	invoked_progress = 0;

	ionotif_register(&progress_changed);
}

static void
teardown(void)
{
	ionotif_register(NULL);

	ioeta_free(estim);
}

static void
test_cp_file_invokes_ionotif(void)
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

static void
test_mv_file_invokes_ionotif(void)
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

static void
test_rm_file_invokes_ionotif(void)
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

static void
test_cp_dir_invokes_ionotif(void)
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

static void
test_mv_dir_invokes_ionotif(void)
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

static void
test_rm_dir_invokes_ionotif(void)
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

void
invoked_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	/* Each of the following tests rely on the previous one. */
	run_test(test_cp_file_invokes_ionotif);
	run_test(test_mv_file_invokes_ionotif);
	run_test(test_rm_file_invokes_ionotif);

	/* Each of the following tests rely on the previous one. */
	run_test(test_cp_dir_invokes_ionotif);
	run_test(test_mv_dir_invokes_ionotif);
	run_test(test_rm_dir_invokes_ionotif);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
