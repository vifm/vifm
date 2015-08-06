#include "utils.h"

#include <stic.h>

#include <unistd.h> /* F_OK access() chdir() */

#include <stdio.h> /* FILE fclose() fopen() */

#include "../../src/compat/os.h"
#include "../../src/io/iop.h"
#include "../../src/io/ior.h"
#include "../../src/utils/fs.h"

void
create_non_empty_dir(const char dir[], const char file[])
{
	create_empty_dir(dir);

	assert_success(chdir(dir));
	create_empty_file(file);
	assert_success(chdir(".."));
}

void
create_empty_nested_dir(const char dir[],
		const char nested_dir[])
{
	create_empty_dir(dir);

	assert_success(chdir(dir));
	create_empty_dir(nested_dir);
	assert_success(chdir(".."));
}

void
create_non_empty_nested_dir(const char root_dir[], const char nested_dir[],
		const char file[])
{
	create_empty_dir(root_dir);

	assert_success(chdir(root_dir));
	{
		create_empty_dir(nested_dir);

		assert_success(chdir(nested_dir));
		create_empty_file(file);
		assert_success(chdir(".."));
	}
	assert_success(chdir(".."));
}

void
create_empty_dir(const char dir[])
{
	os_mkdir(dir, 0700);
	assert_true(is_dir(dir));
}

void
create_empty_file(const char file[])
{
	FILE *const f = fopen(file, "w");
	if(f != NULL)
	{
		fclose(f);
	}
	assert_success(access(file, F_OK));
}

void
clone_file(const char src[], const char dst[])
{
	io_args_t args = {
		.arg1.src = src,
		.arg2.dst = dst,
	};
	assert_success(iop_cp(&args));
}

void
delete_file(const char name[])
{
	io_args_t args = {
		.arg1.path = name,
	};
	assert_success(iop_rmfile(&args));
}

void
delete_dir(const char name[])
{
	io_args_t args = {
		.arg1.path = name,
	};
	assert_success(iop_rmdir(&args));
}

void
delete_tree(const char name[])
{
	io_args_t args = {
		.arg1.path = name,
	};
	assert_success(ior_rm(&args));
}

int
file_exists(const char file[])
{
	return access(file, F_OK) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
