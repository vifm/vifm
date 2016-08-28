#include <stic.h>

#include <string.h>

#include "../../src/ui/column_view.h"
#include "../../src/viewcolumns_parser.h"

#include "test.h"

column_info_t info;

DEFINE_SUITE();

static void
add_column(columns_t *columns, column_info_t column_info)
{
	info = column_info;
}

static int
map_name(const char name[], void *arg)
{
	(void)arg;
	return (strcmp(name, "name") == 0) ? 0 : -1;
}

int
do_parse(const char *str)
{
	return parse_columns(NULL, add_column, map_name, str, NULL);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
