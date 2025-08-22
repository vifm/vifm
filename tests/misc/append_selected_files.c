#include <stic.h>

#include <stdlib.h>
#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/macros.h"

static void reset_buf(str_buf_t *buf, const char value[]);

static iter_func iter = &iter_marked_entries;

SETUP()
{
	/* lwin */
	view_setup(&lwin);
	strcpy(lwin.curr_dir, "/lwin");
	append_view_entry(&lwin, "lfile0")->marked = 1;
	append_view_entry(&lwin, "lfile1");
	append_view_entry(&lwin, "lfile2")->marked = 1;
	append_view_entry(&lwin, "lfile3");
	lwin.list_pos = 2;

	/* rwin */
	view_setup(&rwin);
	strcpy(rwin.curr_dir, "/rwin");
	append_view_entry(&rwin, "rfile0");
	append_view_entry(&rwin, "rfile1")->marked = 1;
	append_view_entry(&rwin, "rfile2");
	append_view_entry(&rwin, "rfile3")->marked = 1;
	append_view_entry(&rwin, "rfile4");
	append_view_entry(&rwin, "rfile5")->marked = 1;
	append_view_entry(&rwin, "rdir6")->marked = 1;
	rwin.dir_entry[6].type = FT_DIR;
	rwin.list_pos = 5;

	curr_view = &lwin;
	other_view = &rwin;
}

TEARDOWN()
{
	view_teardown(&lwin);
	view_teardown(&rwin);
}

TEST(f)
{
	str_buf_t expanded = {};

	reset_buf(&expanded, "");
	append_selected_files(&expanded, &lwin, 0, 0, "", iter, 1);
	assert_string_equal("lfile0 lfile2", expanded.data);

	reset_buf(&expanded, "/");
	append_selected_files(&expanded, &lwin, 0, 0, "", iter, 1);
	assert_string_equal("/lfile0 lfile2", expanded.data);

	reset_buf(&expanded, "");
	append_selected_files(&expanded, &rwin, 0, 0, "", iter, 1);
	assert_string_equal(SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 "
	                    SL "rwin" SL "rfile5 " SL "rwin" SL "rdir6",
			expanded.data);

	reset_buf(&expanded, "/");
	append_selected_files(&expanded, &rwin, 0, 0, "", iter, 1);
	assert_string_equal("/" SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 "
	                    SL "rwin" SL "rfile5 " SL "rwin" SL "rdir6",
			expanded.data);

	free(expanded.data);
}

TEST(c)
{
	str_buf_t expanded = {};

	reset_buf(&expanded, "");
	append_selected_files(&expanded, &lwin, 1, 0, "", iter, 1);
	assert_string_equal("lfile2", expanded.data);

	reset_buf(&expanded, "/");
	append_selected_files(&expanded, &lwin, 1, 0, "", iter, 1);
	assert_string_equal("/lfile2", expanded.data);

	reset_buf(&expanded, "");
	append_selected_files(&expanded, &rwin, 1, 0, "", iter, 1);
	assert_string_equal("" SL "rwin" SL "rfile5", expanded.data);

	reset_buf(&expanded, "/");
	append_selected_files(&expanded, &rwin, 1, 0, "", iter, 1);
	assert_string_equal("/" SL "rwin" SL "rfile5", expanded.data);

	free(expanded.data);
}

static void
reset_buf(str_buf_t *buf, const char value[])
{
	free(buf->data);

	buf->data = strdup(value);
	buf->len = strlen(value);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
