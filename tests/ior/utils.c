#include "utils.h"

#include <stic.h>

#include <unistd.h> /* F_OK access() chdir() */

#include <stdio.h> /* FILE fclose() fopen() */

#include "../../src/utils/fs.h"

void
create_non_empty_dir(const char dir[], const char file[])
{
	create_empty_dir(dir);

	assert_int_equal(0, chdir(dir));
	create_empty_file(file);
	assert_int_equal(0, chdir(".."));
}

void
create_empty_nested_dir(const char dir[],
		const char nested_dir[])
{
	create_empty_dir(dir);

	assert_int_equal(0, chdir(dir));
	create_empty_dir(nested_dir);
	assert_int_equal(0, chdir(".."));
}

void
create_non_empty_nested_dir(const char root_dir[], const char nested_dir[],
		const char file[])
{
	create_empty_dir(root_dir);

	assert_int_equal(0, chdir(root_dir));
	{
		create_empty_dir(nested_dir);

		assert_int_equal(0, chdir(nested_dir));
		create_empty_file(file);
		assert_int_equal(0, chdir(".."));
	}
	assert_int_equal(0, chdir(".."));
}

void
create_empty_dir(const char dir[])
{
	make_dir(dir, 0700);
	assert_true(is_dir(dir));
}

void
create_empty_file(const char file[])
{
	FILE *const f = fopen(file, "w");
	fclose(f);
	assert_int_equal(0, access(file, F_OK));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
