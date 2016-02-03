#include <stic.h>

#include <string.h>

#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"

#ifndef _WIN32
#define ROOT "/"
#else
#define ROOT "C:/"
#endif

TEST(empty_path)
{
	strcpy(lwin.curr_dir, "");
	leave_invalid_dir(&lwin);
	assert_success(str_to_upper(lwin.curr_dir, lwin.curr_dir,
				strlen(lwin.curr_dir) + 1));
	assert_string_equal(ROOT, lwin.curr_dir);
}

TEST(wrong_path_without_trailing_slash)
{
	strcpy(lwin.curr_dir, "/aaaaaaaaaaa/bbbbbbbbbbb/cccccccccc");
	leave_invalid_dir(&lwin);
	assert_success(str_to_upper(lwin.curr_dir, lwin.curr_dir,
				strlen(lwin.curr_dir) + 1));
	assert_string_equal(ROOT, lwin.curr_dir);
}

TEST(wrong_path_with_trailing_slash)
{
	strcpy(lwin.curr_dir, "/aaaaaaaaaaa/bbbbbbbbbbb/cccccccccc/");
	leave_invalid_dir(&lwin);
	assert_success(str_to_upper(lwin.curr_dir, lwin.curr_dir,
				strlen(lwin.curr_dir) + 1));
	assert_string_equal(ROOT, lwin.curr_dir);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
