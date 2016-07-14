#include "utils.h"

#include <stic.h>

#include <unistd.h> /* access() */

#include <stdio.h> /* FILE fclose() fopen() */

#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/filelist.h"

void
view_setup(FileView *view)
{
	view->list_rows = 0;
	view->dir_entry = NULL;

	assert_success(filter_init(&view->local_filter.filter, 1));
	assert_success(filter_init(&view->manual_filter, 1));
	assert_success(filter_init(&view->auto_filter, 1));
}

void
view_teardown(FileView *view)
{
	int i;

	for(i = 0; i < lwin.list_rows; ++i)
	{
		free_dir_entry(&lwin, &lwin.dir_entry[i]);
	}
	dynarray_free(lwin.dir_entry);
}

void
create_empty_file(const char path[])
{
	FILE *const f = fopen(path, "w");
	fclose(f);
	assert_success(access(path, F_OK));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
