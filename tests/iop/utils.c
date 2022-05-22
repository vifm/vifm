#include "utils.h"

#include <stic.h>

#include <stdio.h> /* EOF FILE fclose() fgetc() fopen() */

#include "../../src/io/iop.h"

void
create_test_file(const char name[])
{
	io_args_t args = {
		.arg1.path = name,
	};
	ioe_errlst_init(&args.result.errors);

	assert_int_equal(IO_RES_SUCCEEDED, iop_mkfile(&args));
	assert_int_equal(0, args.result.errors.error_count);
}

void
clone_test_file(const char src[], const char dst[])
{
	io_args_t args = {
		.arg1.src = src,
		.arg2.dst = dst,
	};
	ioe_errlst_init(&args.result.errors);

	assert_int_equal(IO_RES_SUCCEEDED, iop_cp(&args));
	assert_int_equal(0, args.result.errors.error_count);
}

void
delete_test_file(const char name[])
{
	io_args_t args = {
		.arg1.path = name,
	};
	ioe_errlst_init(&args.result.errors);

	assert_int_equal(IO_RES_SUCCEEDED, iop_rmfile(&args));
	assert_int_equal(0, args.result.errors.error_count);

	ioe_errlst_free(&args.result.errors);
}

int
files_are_identical(const char a[], const char b[])
{
	FILE *const a_file = fopen(a, "rb");
	FILE *const b_file = fopen(b, "rb");
	int a_data, b_data;

	if(a_file == NULL || b_file == NULL)
	{
		if(a_file != NULL)
		{
			fclose(a_file);
		}
		if(b_file != NULL)
		{
			fclose(b_file);
		}
		return 0;
	}

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
/* vim: set cinoptions+=t0 filetype=c : */
