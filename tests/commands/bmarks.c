#include <stic.h>

#include <stddef.h> /* NULL */
#include <string.h> /* strcmp() strcpy() strlen() */

#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/bmarks.h"
#include "../../src/cmd_core.h"

static int count_bmarks(void);
static void bmarks_cb(const char p[], const char t[], time_t timestamp,
		void *arg);

static char *path;
static char *tags;
static int cb_called;

SETUP()
{
	cmds_init();
	lwin.selected_files = 0;
	strcpy(lwin.curr_dir, "/a/path");
	path = NULL;
	tags = NULL;

	curr_view = &lwin;
	other_view = &rwin;
}

TEARDOWN()
{
	assert_success(cmds_dispatch("delbmarks!", &lwin, CIT_COMMAND));
	free(path);
	free(tags);
}

TEST(tag_with_comma_is_rejected)
{
	assert_failure(cmds_dispatch("bmark a,b", &lwin, CIT_COMMAND));
}

TEST(tag_with_space_is_rejected)
{
	assert_failure(cmds_dispatch("bmark a\\ b", &lwin, CIT_COMMAND));
}

TEST(emark_with_bookmark_path_only)
{
	assert_failure(cmds_dispatch("bmark! /fake/path", &lwin, CIT_COMMAND));
}

TEST(emark_allows_specifying_bookmark_path)
{
	assert_success(cmds_dispatch("bmark! /fake/path tag", &lwin, CIT_COMMAND));
	assert_int_equal(1, count_bmarks());
}

TEST(delbmarks_with_emark_removes_all_tags)
{
	assert_success(cmds_dispatch("bmark! /path1 tag1", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("bmark! /path2 tag2", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("bmark! /path3 tag3", &lwin, CIT_COMMAND));

	assert_success(cmds_dispatch("delbmarks!", &lwin, CIT_COMMAND));
	assert_int_equal(0, count_bmarks());
}

TEST(delbmarks_with_emark_removes_selected_bookmarks)
{
	assert_success(cmds_dispatch("bmark! /path1 tag1", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("bmark! /path2 tag2", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("bmark! /path3 tag3", &lwin, CIT_COMMAND));

	assert_success(cmds_dispatch("delbmarks! /path1 /path3", &lwin, CIT_COMMAND));
	assert_int_equal(1, count_bmarks());
}

TEST(delbmarks_without_args_removes_current_mark)
{
	assert_success(cmds_dispatch("bmark tag1", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("delbmarks", &lwin, CIT_COMMAND));
	assert_int_equal(0, count_bmarks());
}

TEST(delbmarks_with_args_removes_matching_bookmarks)
{
	assert_success(cmds_dispatch("bmark! /path1 t1 t2", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("bmark! /path2 t2 t3", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("bmark! /path3 t1 t3", &lwin, CIT_COMMAND));

	assert_success(cmds_dispatch("delbmarks t3", &lwin, CIT_COMMAND));
	assert_int_equal(1, count_bmarks());
}

TEST(delbmarks_tag_with_comma_is_rejected)
{
	assert_failure(cmds_dispatch("delbmarks a,b", &lwin, CIT_COMMAND));
}

TEST(arguments_are_unescaped)
{
	assert_success(cmds_dispatch("bmark! /\\*stars\\* tag", &lwin, CIT_COMMAND));
	assert_int_equal(1, count_bmarks());
	assert_string_equal("/*stars*", path);
}

TEST(arguments_are_unquoted_single)
{
	assert_success(cmds_dispatch("bmark! '/squotes' tag", &lwin, CIT_COMMAND));
	assert_int_equal(1, count_bmarks());
	assert_string_equal("/squotes", path);
}

TEST(arguments_are_unquoted_double)
{
	assert_success(cmds_dispatch("bmark! \"/dquotes\" tag", &lwin, CIT_COMMAND));
	assert_int_equal(1, count_bmarks());
	assert_string_equal("/dquotes", path);
}

TEST(first_argument_is_expanded)
{
	assert_success(cmds_dispatch("bmark! %d tag", &lwin, CIT_COMMAND));
	assert_int_equal(1, count_bmarks());
	assert_string_equal("/a/path", path);
}

TEST(not_first_argument_is_not_expanded)
{
	assert_success(cmds_dispatch("bmark! /dir %d", &lwin, CIT_COMMAND));
	assert_int_equal(1, count_bmarks());
	assert_string_equal("%d", tags);
}

TEST(not_all_macros_are_expanded)
{
	assert_success(cmds_dispatch("bmark! /%b%n%i%a%m%M%s%S%u%U%px tag", &lwin,
				CIT_COMMAND));
	assert_int_equal(1, count_bmarks());
	assert_string_equal("/", path);
}

TEST(tilde_is_expanded)
{
	assert_success(cmds_dispatch("bmark! ~ tag", &lwin, CIT_COMMAND));
	assert_int_equal(1, count_bmarks());
	assert_false(path[0] == '~');
	assert_false(path[strlen(path) - 1] == '~');
}

static int
count_bmarks(void)
{
	cb_called = 0;
	bmarks_list(&bmarks_cb, NULL);
	return cb_called;
}

static void
bmarks_cb(const char p[], const char t[], time_t timestamp, void *arg)
{
	replace_string(&path, p);
	replace_string(&tags, t);
	++cb_called;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
