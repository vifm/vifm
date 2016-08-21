#include "utils.h"

#include <stic.h>

#include <unistd.h> /* access() */

#include <stdio.h> /* FILE fclose() fopen() */
#include <string.h> /* snprintf() strcpy() */

#include "../../src/compat/os.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/fswatch.h"
#include "../../src/utils/path.h"
#include "../../src/background.h"
#include "../../src/filelist.h"

void
view_setup(FileView *view)
{
	view->list_rows = 0;
	view->dir_entry = NULL;

	assert_success(filter_init(&view->local_filter.filter, 1));
	assert_success(filter_init(&view->manual_filter, 1));
	assert_success(filter_init(&view->auto_filter, 1));

	view->sort[0] = SK_NONE;
	ui_view_sort_list_ensure_well_formed(view, view->sort);
}

void
view_teardown(FileView *view)
{
	int i;

	for(i = 0; i < view->list_rows; ++i)
	{
		free_dir_entry(view, &view->dir_entry[i]);
	}
	dynarray_free(view->dir_entry);

	filter_dispose(&view->local_filter.filter);
	filter_dispose(&view->manual_filter);
	filter_dispose(&view->auto_filter);

	fswatch_free(view->watch);
	view->watch = NULL;
}

void
create_empty_file(const char path[])
{
	FILE *const f = fopen(path, "w");
	fclose(f);
	assert_success(access(path, F_OK));
}

void
create_empty_dir(const char path[])
{
	os_mkdir(path, 0700);
	assert_true(is_dir(path));
}

void
wait_for_bg(void)
{
	int counter = 0;
	while(bg_has_active_jobs())
	{
		usleep(2000);
		if(++counter > 100)
		{
			assert_fail("Waiting for too long.");
			break;
		}
	}
}

void
set_to_sandbox_path(char buf[], size_t buf_len)
{
	if(is_path_absolute(SANDBOX_PATH))
	{
		strcpy(buf, SANDBOX_PATH);
	}
	else
	{
		char cwd[PATH_MAX];
		assert_non_null(get_cwd(cwd, sizeof(cwd)));
		snprintf(buf, buf_len, "%s/%s", cwd, SANDBOX_PATH);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
