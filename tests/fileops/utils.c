#include "utils.h"

#include <stic.h>

#include <unistd.h> /* access() usleep() */

#include <stdio.h> /* FILE fclose() fopen() */
#include <string.h> /* snprintf() strcpy() */

#include "../../src/compat/os.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/fswatch.h"
#include "../../src/utils/matcher.h"
#include "../../src/utils/path.h"
#include "../../src/background.h"
#include "../../src/filelist.h"

void
view_setup(view_t *view)
{
	char *error;

	view->list_rows = 0;
	view->dir_entry = NULL;

	assert_success(filter_init(&view->local_filter.filter, 1));
	assert_non_null(view->manual_filter = matcher_alloc("", 0, 0, "", &error));
	assert_success(filter_init(&view->auto_filter, 1));

	view->sort[0] = SK_NONE;
	ui_view_sort_list_ensure_well_formed(view, view->sort);
}

void
view_teardown(view_t *view)
{
	flist_free_view(view);
}

void
create_empty_file(const char path[])
{
	FILE *const f = fopen(path, "w");
	if(f != NULL)
	{
		fclose(f);
	}
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
	while(bg_has_active_jobs(0))
	{
		usleep(5000);
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
		char cwd[PATH_MAX + 1];
		assert_non_null(get_cwd(cwd, sizeof(cwd)));
		snprintf(buf, buf_len, "%s/%s", cwd, SANDBOX_PATH);
	}
}

void
make_abs_path(char buf[], size_t buf_len, const char base[], const char sub[],
		const char cwd[])
{
	char local_buf[buf_len];

	if(is_path_absolute(base))
	{
		snprintf(local_buf, buf_len, "%s%s%s", base, (sub[0] == '\0' ? "" : "/"),
				sub);
	}
	else
	{
		snprintf(local_buf, buf_len, "%s/%s%s%s", cwd, base,
				(sub[0] == '\0' ? "" : "/"), sub);
	}

	canonicalize_path(local_buf, buf, buf_len);
	if(!ends_with_slash(sub) && !is_root_dir(buf))
	{
		chosp(buf);
	}
}

int
not_windows(void)
{
#ifdef _WIN32
	return 0;
#else
	return 1;
#endif
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
