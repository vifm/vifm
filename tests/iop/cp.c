#include <stic.h>

#include <stdio.h> /* EOF FILE fclose() fopen() fread() */

#include <sys/types.h> /* stat */
#include <sys/stat.h> /* stat */
#include <unistd.h> /* lstat() */

#include "../../src/io/iop.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/fs_limits.h"
#include "../../src/utils/utils.h"

static int not_windows(void);

static int
files_are_identical(const char a[], const char b[])
{
	FILE *const a_file = fopen(a, "rb");
	FILE *const b_file = fopen(b, "rb");
	int a_data, b_data;

	do
	{
		a_data = fgetc(a_file);
		b_data = fgetc(b_file);
	}
	while(a_data != EOF && b_data != EOF);

	fclose(b_file);
	fclose(a_file);

	return a_data == b_data && a_data == EOF;
}

TEST(dir_is_not_copied)
{
	io_args_t args =
	{
		.arg1.src = "../existing-files",
		.arg2.dst = "existing-files",
	};
	assert_false(iop_cp(&args) == 0);
}

TEST(empty_file_is_copied)
{
	{
		io_args_t args =
		{
			.arg1.path = "empty",
		};
		assert_int_equal(0, iop_mkfile(&args));
	}

	{
		io_args_t args =
		{
			.arg1.src = "empty",
			.arg2.dst = "empty-copy",
		};
		assert_int_equal(0, iop_cp(&args));
	}

	assert_true(files_are_identical("empty", "empty-copy"));

	{
		io_args_t args =
		{
			.arg1.path = "empty",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "empty-copy",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

TEST(file_is_not_overwritten_if_not_asked)
{
	{
		io_args_t args =
		{
			.arg1.path = "empty",
		};
		assert_int_equal(0, iop_mkfile(&args));
	}

	{
		io_args_t args =
		{
			.arg1.src = "empty",
			.arg2.dst = "empty-copy",
		};
		assert_int_equal(0, iop_cp(&args));
	}

	{
		io_args_t args =
		{
			.arg1.src = "../read/two-lines",
			.arg2.dst = "empty-copy",
			.arg3.crs = IO_CRS_FAIL,
		};
		assert_false(iop_cp(&args) == 0);
	}

	assert_true(files_are_identical("empty", "empty-copy"));
	assert_false(files_are_identical("../read/two-lines", "empty-copy"));

	{
		io_args_t args =
		{
			.arg1.path = "empty",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "empty-copy",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

TEST(file_is_overwritten_if_asked)
{
	{
		io_args_t args =
		{
			.arg1.src = "../read/two-lines",
			.arg2.dst = "two-lines",
			.arg3.crs = IO_CRS_FAIL,
		};
		assert_int_equal(0, iop_cp(&args));
	}

	assert_false(files_are_identical("../read/binary-data", "two-lines"));

	{
		io_args_t args =
		{
			.arg1.src = "../read/binary-data",
			.arg2.dst = "two-lines",
			.arg3.crs = IO_CRS_REPLACE_FILES,
		};
		assert_int_equal(0, iop_cp(&args));
	}

	assert_true(files_are_identical("../read/binary-data", "two-lines"));

	{
		io_args_t args =
		{
			.arg1.path = "two-lines",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

static void
file_is_copied(const char original[])
{
	{
		io_args_t args =
		{
			.arg1.src = original,
			.arg2.dst = "copy",
		};
		assert_int_equal(0, iop_cp(&args));
	}

	assert_true(files_are_identical("copy", original));

	{
		io_args_t args =
		{
			.arg1.path = "copy",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

TEST(block_size_file_is_copied)
{
	file_is_copied("../various-sizes/block-size-file");
}

TEST(block_size_minus_one_file_is_copied)
{
	file_is_copied("../various-sizes/block-size-minus-one-file");
}

TEST(block_size_plus_one_file_is_copied)
{
	file_is_copied("../various-sizes/block-size-plus-one-file");
}

TEST(double_block_size_file_is_copied)
{
	file_is_copied("../various-sizes/double-block-size-file");
}

TEST(double_block_size_minus_one_file_is_copied)
{
	file_is_copied("../various-sizes/double-block-size-minus-one-file");
}

TEST(double_block_size_plus_one_file_is_copied)
{
	file_is_copied("../various-sizes/double-block-size-plus-one-file");
}

TEST(appending_works_for_files)
{
	uint64_t size;

	{
		io_args_t args =
		{
			.arg1.src = "../various-sizes/block-size-minus-one-file",
			.arg2.dst = "appending",
		};
		assert_int_equal(0, iop_cp(&args));
	}

	size = get_file_size("appending");

	{
		io_args_t args =
		{
			.arg1.src = "../various-sizes/block-size-file",
			.arg2.dst = "appending",
			.arg3.crs = IO_CRS_APPEND_TO_FILES,
		};
		assert_int_equal(0, iop_cp(&args));
	}

	assert_int_equal(size + 1, get_file_size("appending"));

	{
		io_args_t args =
		{
			.arg1.src = "../various-sizes/block-size-plus-one-file",
			.arg2.dst = "appending",
			.arg3.crs = IO_CRS_APPEND_TO_FILES,
		};
		assert_int_equal(0, iop_cp(&args));
	}

	assert_int_equal(size + 2, get_file_size("appending"));

	{
		io_args_t args =
		{
			.arg1.path = "appending",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

TEST(appending_does_not_shrink_files)
{
	uint64_t size;

	{
		io_args_t args =
		{
			.arg1.src = "../read/two-lines",
			.arg2.dst = "two-lines",
		};
		assert_int_equal(0, iop_cp(&args));
	}

	size = get_file_size("two-lines");

	{
		io_args_t args =
		{
			.arg1.src = "../existing-files/a",
			.arg2.dst = "two-lines",
			.arg3.crs = IO_CRS_APPEND_TO_FILES,
		};
		assert_int_equal(0, iop_cp(&args));
	}

	assert_int_equal(size, get_file_size("two-lines"));

	{
		io_args_t args =
		{
			.arg1.path = "two-lines",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

/* Windows doesn't support Unix-style permissions. */
TEST(file_permissions_are_preserved, IF(not_windows))
{
	struct stat src;
	struct stat dst;

	{
		io_args_t args =
		{
			.arg1.path = "file",
		};
		assert_int_equal(0, iop_mkfile(&args));
	}

	assert_int_equal(0, chmod("file", 0200));

	assert_int_equal(0, lstat("file", &src));
	assert_false((src.st_mode & 0777) == 0600);

	assert_int_equal(0, chmod("file", 0600));

	{
		io_args_t args =
		{
			.arg1.src = "file",
			.arg2.dst = "file-copy",
		};
		assert_int_equal(0, iop_cp(&args));
	}

	assert_int_equal(0, lstat("file", &src));
	assert_int_equal(0, lstat("file-copy", &dst));
	assert_int_equal(src.st_mode & 0777, dst.st_mode & 0777);

	{
		io_args_t args =
		{
			.arg1.path = "file",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "file-copy",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

/* Creating symbolic links on Windows requires administrator rights. */
TEST(file_symlink_copy_is_symlink, IF(not_windows))
{
	char old_target[PATH_MAX];
	char new_target[PATH_MAX];

	{
		io_args_t args =
		{
			.arg1.path = "../read/two-lines",
			.arg2.target = "sym-link",
		};
		assert_int_equal(0, iop_ln(&args));
	}

	assert_true(is_symlink("sym-link"));

	{
		io_args_t args =
		{
			.arg1.src = "sym-link",
			.arg2.dst = "sym-link-copy",
		};
		assert_int_equal(0, iop_cp(&args));
	}

	assert_true(is_symlink("sym-link"));
	assert_true(is_symlink("sym-link-copy"));

	assert_int_equal(0,
			get_link_target("sym-link", old_target, sizeof(old_target)));
	assert_int_equal(0,
			get_link_target("sym-link-copy", new_target, sizeof(new_target)));

	assert_string_equal(new_target, old_target);

	{
		io_args_t args =
		{
			.arg1.path = "sym-link",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "sym-link-copy",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

/* Creating symbolic links on Windows requires administrator rights. */
TEST(dir_symlink_copy_is_symlink, IF(not_windows))
{
	make_dir("dir", 0700);
	assert_true(is_dir("dir"));

	{
		io_args_t args =
		{
			.arg1.path = "dir",
			.arg2.target = "dir-sym-link",
		};
		assert_int_equal(0, iop_ln(&args));
	}

	assert_true(is_symlink("dir-sym-link"));
	assert_true(is_dir("dir-sym-link"));

	{
		io_args_t args =
		{
			.arg1.src = "dir-sym-link",
			.arg2.dst = "dir-sym-link-copy",
		};
		assert_int_equal(0, iop_cp(&args));
	}

	assert_true(is_symlink("dir-sym-link"));
	assert_true(is_dir("dir-sym-link"));
	assert_true(is_symlink("dir-sym-link-copy"));
	assert_true(is_dir("dir-sym-link-copy"));

	{
		io_args_t args =
		{
			.arg1.path = "dir-sym-link",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "dir-sym-link-copy",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "dir",
		};
		assert_int_equal(0, iop_rmdir(&args));
	}
}

static int
not_windows(void)
{
	return get_env_type() != ET_WIN;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
