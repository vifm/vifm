#include "test.h"

#include <string.h>

#include "seatest.h"

#include "../../src/column_view.h"
#include "../../src/viewcolumns_parser.h"

void alignment_tests(void);
void cropping_tests(void);
void sizes_tests(void);
void syntax_tests(void);

column_info_t info;

static void
add_column(columns_t columns, column_info_t column_info)
{
	info = column_info;
}

static int
map_name(const char *name)
{
	return (strcmp(name, "name") == 0) ? 0 : -1;
}

int
do_parse(const char *str)
{
	return parse_columns(NULL_COLUMNS, add_column, map_name, str);
}

static void
all_tests(void)
{
	alignment_tests();
	cropping_tests();
	sizes_tests();
	syntax_tests();
}

static void
setup(void)
{
}

static void
teardown(void)
{
}

int
main(int argc, char **argv)
{
	suite_setup(setup);
	suite_teardown(teardown);

	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
