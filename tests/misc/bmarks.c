#include <stic.h>

#include <stddef.h> /* NULL */
#include <string.h> /* strcmp() */

#include "../../src/ui/ui.h"
#include "../../src/bmarks.h"
#include "../../src/commands.h"

static int count_bmarks(void);
static void bmarks_cb(const char path[], const char tags[], time_t timestamp,
		void *arg);

static int cb_called;

SETUP()
{
	init_commands();
	lwin.selected_files = 0;
}

TEARDOWN()
{
	assert_success(exec_commands("delbmarks!", &lwin, CIT_COMMAND));
}

TEST(tag_with_comma_is_rejected)
{
	assert_failure(exec_commands("bmark a,b", &lwin, CIT_COMMAND));
}

TEST(tag_with_space_is_rejected)
{
	assert_failure(exec_commands("bmark a\\ b", &lwin, CIT_COMMAND));
}

TEST(emark_allows_specifying_bookmark_path)
{
	assert_success(exec_commands("bmark! /fake/path tag", &lwin, CIT_COMMAND));
	assert_int_equal(1, count_bmarks());
}

TEST(delbmarks_with_emark_removes_all_tags)
{
	assert_success(exec_commands("bmark! /path1 tag1", &lwin, CIT_COMMAND));
	assert_success(exec_commands("bmark! /path2 tag2", &lwin, CIT_COMMAND));
	assert_success(exec_commands("bmark! /path3 tag3", &lwin, CIT_COMMAND));

	assert_success(exec_commands("delbmarks!", &lwin, CIT_COMMAND));
	assert_int_equal(0, count_bmarks());
}

TEST(delbmarks_with_emark_removes_selected_bookmarks)
{
	assert_success(exec_commands("bmark! /path1 tag1", &lwin, CIT_COMMAND));
	assert_success(exec_commands("bmark! /path2 tag2", &lwin, CIT_COMMAND));
	assert_success(exec_commands("bmark! /path3 tag3", &lwin, CIT_COMMAND));

	assert_success(exec_commands("delbmarks! /path1 /path3", &lwin, CIT_COMMAND));
	assert_int_equal(1, count_bmarks());
}

TEST(delbmarks_without_args_removes_current_mark)
{
	assert_success(exec_commands("bmark tag1", &lwin, CIT_COMMAND));
	assert_success(exec_commands("delbmarks", &lwin, CIT_COMMAND));
	assert_int_equal(0, count_bmarks());
}

TEST(delbmarks_with_args_removes_matching_bookmarks)
{
	assert_success(exec_commands("bmark! /path1 t1 t2", &lwin, CIT_COMMAND));
	assert_success(exec_commands("bmark! /path2 t2 t3", &lwin, CIT_COMMAND));
	assert_success(exec_commands("bmark! /path3 t1 t3", &lwin, CIT_COMMAND));

	assert_success(exec_commands("delbmarks t3", &lwin, CIT_COMMAND));
	assert_int_equal(1, count_bmarks());
}

static int
count_bmarks(void)
{
	cb_called = 0;
	bmarks_list(&bmarks_cb, NULL);
	return cb_called;
}

static void
bmarks_cb(const char path[], const char tags[], time_t timestamp, void *arg)
{
	++cb_called;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
