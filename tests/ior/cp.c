#include "seatest.h"

#include <unistd.h> /* F_OK access() */

#include "../../src/io/iop.h"
#include "../../src/io/ior.h"
#include "../../src/utils/fs.h"

static void
test_file_is_copied(void)
{
	{
		io_args_t args =
		{
			.arg1.src = "../read/binary-data",
			.arg2.dst = "binary-data",
		};
		assert_int_equal(0, ior_cp(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "binary-data",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

static void
test_empty_directory_is_copied(void)
{
	make_dir("empty-dir", 0700);
	assert_int_equal(0, access("empty-dir", F_OK));

	{
		io_args_t args =
		{
			.arg1.src = "empty-dir",
			.arg2.dst = "empty-dir-copy",
		};
		assert_int_equal(0, ior_cp(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "empty-dir",
		};
		assert_int_equal(0, iop_rmdir(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "empty-dir-copy",
		};
		assert_int_equal(0, iop_rmdir(&args));
	}
}

static void
test_non_empty_directory_is_copied(void)
{
	make_dir("non-empty-dir", 0700);
	assert_int_equal(0, access("non-empty-dir", F_OK));

	assert_int_equal(0, chdir("non-empty-dir"));
	{
		FILE *const f = fopen("a-file", "w");
		fclose(f);
		assert_int_equal(0, access("a-file", F_OK));
	}
	assert_int_equal(0, chdir(".."));

	{
		io_args_t args =
		{
			.arg1.src = "non-empty-dir",
			.arg2.dst = "non-empty-dir-copy",
		};
		assert_int_equal(0, ior_cp(&args));
	}

	assert_int_equal(0, access("non-empty-dir-copy/a-file", F_OK));

	{
		io_args_t args =
		{
			.arg1.path = "non-empty-dir",
		};
		assert_int_equal(0, ior_rm(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "non-empty-dir-copy",
		};
		assert_int_equal(0, ior_rm(&args));
	}
}

static void
test_empty_nested_directory_is_copied(void)
{
	make_dir("non-empty-dir", 0700);
	assert_int_equal(0, access("non-empty-dir", F_OK));

	assert_int_equal(0, chdir("non-empty-dir"));
	{
		make_dir("empty-nested-dir", 0700);
		assert_int_equal(0, access("empty-nested-dir", F_OK));
	}
	assert_int_equal(0, chdir(".."));

	{
		io_args_t args =
		{
			.arg1.src = "non-empty-dir",
			.arg2.dst = "non-empty-dir-copy",
		};
		assert_int_equal(0, ior_cp(&args));
	}

	assert_int_equal(0,
			access("non-empty-dir-copy/empty-nested-dir", F_OK));

	{
		io_args_t args =
		{
			.arg1.path = "non-empty-dir",
		};
		assert_int_equal(0, ior_rm(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "non-empty-dir-copy",
		};
		assert_int_equal(0, ior_rm(&args));
	}
}

static void
test_non_empty_nested_directory_is_copied(void)
{
	make_dir("non-empty-dir", 0700);
	assert_int_equal(0, access("non-empty-dir", F_OK));

	assert_int_equal(0, chdir("non-empty-dir"));
	{
		make_dir("nested-dir", 0700);
		assert_int_equal(0, access("nested-dir", F_OK));

		assert_int_equal(0, chdir("nested-dir"));
		{
			FILE *const f = fopen("a-file", "w");
			fclose(f);
			assert_int_equal(0, access("a-file", F_OK));
		}
		assert_int_equal(0, chdir(".."));
	}
	assert_int_equal(0, chdir(".."));

	{
		io_args_t args =
		{
			.arg1.src = "non-empty-dir",
			.arg2.dst = "non-empty-dir-copy",
		};
		assert_int_equal(0, ior_cp(&args));
	}

	assert_int_equal(0,
			access("non-empty-dir-copy/nested-dir/a-file", F_OK));

	{
		io_args_t args =
		{
			.arg1.path = "non-empty-dir",
		};
		assert_int_equal(0, ior_rm(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "non-empty-dir-copy",
		};
		assert_int_equal(0, ior_rm(&args));
	}
}

static void
test_fails_to_overwrite_file_by_default(void)
{
	{
		FILE *const f = fopen("a-file", "w");
		fclose(f);
		assert_int_equal(0, access("a-file", F_OK));
	}

	{
		io_args_t args =
		{
			.arg1.src = "../read/two-lines",
			.arg2.dst = "a-file",
		};
		assert_false(ior_cp(&args) == 0);
	}

	{
		io_args_t args =
		{
			.arg1.path = "a-file",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

static void
test_fails_to_overwrite_dir_by_default(void)
{
	make_dir("empty-dir", 0700);
	assert_int_equal(0, access("empty-dir", F_OK));

	{
		io_args_t args =
		{
			.arg1.src = "../read",
			.arg2.dst = "empty-dir",
		};
		assert_false(ior_cp(&args) == 0);
	}

	{
		io_args_t args =
		{
			.arg1.path = "empty-dir",
		};
		assert_int_equal(0, iop_rmdir(&args));
	}
}

static void
test_overwrites_file_when_asked(void)
{
	{
		FILE *const f = fopen("a-file", "w");
		fclose(f);
		assert_int_equal(0, access("a-file", F_OK));
	}

	{
		io_args_t args =
		{
			.arg1.src = "../read/two-lines",
			.arg2.dst = "a-file",
			.arg3.overwrite = 1,
		};
		assert_int_equal(0, ior_cp(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "a-file",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

static void
test_overwrites_dir_when_asked(void)
{
	make_dir("dir", 0700);
	assert_int_equal(0, access("dir", F_OK));

	{
		io_args_t args =
		{
			.arg1.src = "../read",
			.arg2.dst = "dir",
			.arg3.overwrite = 1,
		};
		assert_int_equal(0, ior_cp(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "dir",
		};
		assert_false(iop_rmdir(&args) == 0);
	}

	{
		io_args_t args =
		{
			.arg1.path = "dir",
		};
		assert_int_equal(0, ior_rm(&args));
	}
}

void
cp_tests(void)
{
	test_fixture_start();

	run_test(test_file_is_copied);
	run_test(test_empty_directory_is_copied);
	run_test(test_non_empty_directory_is_copied);
	run_test(test_empty_nested_directory_is_copied);
	run_test(test_non_empty_nested_directory_is_copied);
	run_test(test_fails_to_overwrite_file_by_default);
	run_test(test_fails_to_overwrite_dir_by_default);
	run_test(test_overwrites_file_when_asked);
	run_test(test_overwrites_dir_when_asked);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
