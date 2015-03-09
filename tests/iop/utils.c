#include "utils.h"

#include <stic.h>

#include <stdio.h> /* EOF FILE fclose() fopen() */

#include "../../src/io/iop.h"

void
create_test_file(const char name[])
{
	io_args_t args = {
		.arg1.path = name,
	};
	assert_success(iop_mkfile(&args));
}

void
clone_test_file(const char src[], const char dst[])
{
	io_args_t args = {
		.arg1.src = src,
		.arg2.dst = dst,
	};
	assert_success(iop_cp(&args));
}

void
delete_test_file(const char name[])
{
	io_args_t args = {
		.arg1.path = name,
	};
	assert_success(iop_rmfile(&args));
}

int
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
