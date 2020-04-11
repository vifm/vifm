#include <stic.h>

#include <stdlib.h>
#include <string.h>

#include "../../src/cfg/config.h"
#include "../../src/int/vim.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"

static void teardown_view(view_t *view);

static void
setup_lwin(void)
{
	strcpy(lwin.curr_dir, "/lwin");

	lwin.list_rows = 4;
	lwin.list_pos = 2;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("lfile0");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].name = strdup("lfile1");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];
	lwin.dir_entry[2].name = strdup("lfile2");
	lwin.dir_entry[2].origin = &lwin.curr_dir[0];
	lwin.dir_entry[3].name = strdup("lfile3");
	lwin.dir_entry[3].origin = &lwin.curr_dir[0];

	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[2].selected = 1;
	lwin.selected_files = 2;
}

static void
setup_rwin(void)
{
	strcpy(rwin.curr_dir, "/rwin");

	rwin.list_rows = 6;
	rwin.list_pos = 5;
	rwin.dir_entry = dynarray_cextend(NULL,
			rwin.list_rows*sizeof(*rwin.dir_entry));
	rwin.dir_entry[0].name = strdup("rfile0");
	rwin.dir_entry[0].origin = &rwin.curr_dir[0];
	rwin.dir_entry[1].name = strdup("rfile1");
	rwin.dir_entry[1].origin = &rwin.curr_dir[0];
	rwin.dir_entry[2].name = strdup("rfile2");
	rwin.dir_entry[2].origin = &rwin.curr_dir[0];
	rwin.dir_entry[3].name = strdup("rfile3");
	rwin.dir_entry[3].origin = &rwin.curr_dir[0];
	rwin.dir_entry[4].name = strdup("rfile4");
	rwin.dir_entry[4].origin = &rwin.curr_dir[0];
	rwin.dir_entry[5].name = strdup("rfile5");
	rwin.dir_entry[5].origin = &rwin.curr_dir[0];

	rwin.dir_entry[1].selected = 1;
	rwin.dir_entry[3].selected = 1;
	rwin.dir_entry[5].selected = 1;
	rwin.selected_files = 3;
}

SETUP()
{
	setup_lwin();
	setup_rwin();

	curr_view = &lwin;
	other_view = &rwin;

	update_string(&cfg.vi_command, "vim -p");
	update_string(&cfg.vi_x_command, "");
}

TEARDOWN()
{
	teardown_view(&lwin);
	teardown_view(&rwin);

	update_string(&cfg.vi_command, NULL);
	update_string(&cfg.vi_x_command, NULL);
}

static void
teardown_view(view_t *view)
{
	int i;
	for(i = 0; i < view->list_rows; ++i)
	{
		fentry_free(view, &view->dir_entry[i]);
	}
	dynarray_free(view->dir_entry);
	view->list_rows = 0;
	view->selected_files = 0;
}

TEST(selection)
{
	char *cmd;
	int bg;

	cmd = format_edit_marking_cmd(&bg);
#ifdef _WIN32
	assert_string_equal("vim -p \"lfile0\" \"lfile2\"", cmd);
#else
	assert_string_equal("vim -p lfile0 lfile2", cmd);
#endif
	free(cmd);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
