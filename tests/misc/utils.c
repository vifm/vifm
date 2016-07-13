#include "utils.h"

#include <sys/stat.h> /* chmod() */
#include <unistd.h> /* access() */

#include <stddef.h> /* NULL */
#include <stdio.h> /* fclose() fopen() */
#include <string.h> /* memset() strcpy() */

#include "../../src/cfg/config.h"
#include "../../src/engine/options.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/filtering.h"
#include "../../src/opt_handlers.h"

void
opt_handlers_setup(void)
{
	update_string(&lwin.view_columns, "");
	update_string(&lwin.view_columns_g, "");
	update_string(&lwin.sort_groups, "");
	update_string(&lwin.sort_groups_g, "");
	update_string(&rwin.view_columns, "");
	update_string(&rwin.view_columns_g, "");
	update_string(&rwin.sort_groups, "");
	update_string(&rwin.sort_groups_g, "");

	update_string(&cfg.slow_fs_list, "");
	update_string(&cfg.apropos_prg, "");
	update_string(&cfg.cd_path, "");
	update_string(&cfg.find_prg, "");
	update_string(&cfg.fuse_home, "");
	update_string(&cfg.time_format, "+");
	update_string(&cfg.vi_command, "");
	update_string(&cfg.vi_x_command, "");
	update_string(&cfg.ruler_format, "");
	update_string(&cfg.status_line, "");
	update_string(&cfg.grep_prg, "");
	update_string(&cfg.locate_prg, "");
	update_string(&cfg.border_filler, "");
	update_string(&cfg.shell, "");

	init_option_handlers();
}

void
opt_handlers_teardown(void)
{
	clear_options();

	update_string(&cfg.slow_fs_list, NULL);
	update_string(&cfg.apropos_prg, NULL);
	update_string(&cfg.cd_path, NULL);
	update_string(&cfg.find_prg, NULL);
	update_string(&cfg.fuse_home, NULL);
	update_string(&cfg.time_format, NULL);
	update_string(&cfg.vi_command, NULL);
	update_string(&cfg.vi_x_command, NULL);
	update_string(&cfg.ruler_format, NULL);
	update_string(&cfg.status_line, NULL);
	update_string(&cfg.grep_prg, NULL);
	update_string(&cfg.locate_prg, NULL);
	update_string(&cfg.border_filler, NULL);
	update_string(&cfg.shell, NULL);

	update_string(&lwin.view_columns, NULL);
	update_string(&lwin.view_columns_g, NULL);
	update_string(&lwin.sort_groups, NULL);
	update_string(&lwin.sort_groups_g, NULL);
	update_string(&rwin.view_columns, NULL);
	update_string(&rwin.view_columns_g, NULL);
	update_string(&rwin.sort_groups, NULL);
	update_string(&rwin.sort_groups_g, NULL);
}

void
view_setup(FileView *view)
{
	view->list_rows = 0;
	view->filtered = 0;
	view->list_pos = 0;
	view->dir_entry = NULL;
	view->hide_dot = 0;
	view->invert = 1;
	view->selected_files = 0;

	assert_success(filter_init(&view->local_filter.filter, 1));
	assert_success(filter_init(&view->manual_filter, 1));
	assert_success(filter_init(&view->auto_filter, 1));

	strcpy(view->curr_dir, "/path");
	update_string(&view->custom.orig_dir, NULL);

	view->sort[0] = SK_BY_NAME;
	memset(&view->sort[1], SK_NONE, sizeof(view->sort) - 1);

	view->custom.entry_count = 0;
	view->custom.entries = NULL;
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

	for(i = 0; i < view->custom.entry_count; ++i)
	{
		free_dir_entry(view, &view->custom.entries[i]);
	}
	dynarray_free(view->custom.entries);

	filter_dispose(&view->local_filter.filter);
	filter_dispose(&view->auto_filter);
	filter_dispose(&view->manual_filter);
}

void
create_file(const char path[])
{
	FILE *const f = fopen(path, "w");
	assert_non_null(f);
	if(f != NULL)
	{
		fclose(f);
	}
}

void
create_executable(const char path[])
{
	create_file(path);
	assert_success(access(path, F_OK));
	chmod(path, 0755);
	assert_success(access(path, X_OK));
}

void
make_abs_path(char buf[], size_t buf_len, const char base[], const char sub[],
		const char cwd[])
{
	if(is_path_absolute(base))
	{
		snprintf(buf, buf_len, "%s/%s", base, sub);
	}
	else
	{
		snprintf(buf, buf_len, "%s/%s/%s", cwd, base, sub);
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
/* vim: set cinoptions+=t0 filetype=c : */
