#include <stic.h>

#include <unistd.h> /* chdir() symlink() */

#include <stdlib.h> /* remove() */
#include <string.h> /* strcpy() strdup() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/menus/menus.h"
#include "../../src/modes/modes.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/string_array.h"

static menu_data_t m;

SETUP()
{
	menus_init_data(&m, &lwin, strdup("test"), strdup("No matches"));

	m.len = add_to_string_array(&m.items, m.len, "a");
	m.len = add_to_string_array(&m.items, m.len, "b");
	m.len = add_to_string_array(&m.items, m.len, "c");
}

TEARDOWN()
{
	menus_reset_data(&m);
}

TEST(can_navigate_to_broken_symlink, IF(not_windows))
{
	char *saved_cwd;
	char buf[PATH_MAX + 1];

	strcpy(lwin.curr_dir, ".");

	saved_cwd = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	/* symlink() is not available on Windows, but other code is fine. */
#ifndef _WIN32
	assert_success(symlink("/wrong/path", "broken-link"));
#endif

	make_abs_path(buf, sizeof(buf), SANDBOX_PATH , "broken-link:", saved_cwd);
	/* Were trying to open broken link, which will fail, but the parsing part
	 * should succeed. */
	restore_cwd(saved_cwd);
	assert_success(menus_goto_file(&m, &lwin, buf, 1));

	assert_success(remove(SANDBOX_PATH "/broken-link"));
}

TEST(nothing_is_searched_if_no_pattern)
{
	menus_search_repeat(m.state, 0);
	assert_int_equal(0, menus_search_matched(m.state));
}

TEST(nothing_is_searched_for_wrong_pattern)
{
	menus_search_reset(m.state, 0, 1);
	assert_true(menus_search("*a", &m, 1));
	assert_int_equal(0, menus_search_matched(m.state));
}

TEST(search_via_menu_search)
{
	menus_search_reset(m.state, 0, 1);
	assert_true(menus_search("[abc]", &m, 1));
	assert_int_equal(1, m.pos);
	menus_search_repeat(m.state, 0);
	assert_int_equal(2, m.pos);
}

TEST(ok_to_print_message_if_there_is_no_pattern)
{
	menus_search_print_msg(&m);
}

TEST(ok_to_print_message_for_wrong_pattern)
{
	assert_true(menus_search("*", &m, 1));
	menus_search_print_msg(&m);
}

TEST(forward_found_no_wrap)
{
	cfg.wrap_scan = 0;
	assert_true(menus_search("c", &m, 1));
	assert_int_equal(2, m.pos);
}

TEST(forward_found_wrap)
{
	m.pos = 1;
	cfg.wrap_scan = 1;
	assert_true(menus_search("a", &m, 1));
	assert_int_equal(0, m.pos);
}

TEST(forward_not_found_no_wrap)
{
	cfg.wrap_scan = 0;
	assert_true(menus_search("d", &m, 1));
	assert_int_equal(0, m.pos);
}

TEST(forward_not_found_wrap)
{
	cfg.wrap_scan = 1;
	assert_true(menus_search("d", &m, 1));
	assert_int_equal(0, m.pos);
}

TEST(forward_find_next_no_wrap)
{
	cfg.wrap_scan = 0;
	assert_true(menus_search(".", &m, 1));
	assert_int_equal(1, m.pos);
	assert_true(menus_search(".", &m, 1));
	assert_int_equal(2, m.pos);
	assert_true(menus_search(".", &m, 1));
	assert_int_equal(2, m.pos);
}

TEST(forward_find_next_wrap)
{
	cfg.wrap_scan = 1;
	assert_true(menus_search(".", &m, 1));
	assert_int_equal(1, m.pos);
	assert_true(menus_search(".", &m, 1));
	assert_int_equal(2, m.pos);
	assert_true(menus_search(".", &m, 1));
	assert_int_equal(0, m.pos);
}

TEST(backward_found_no_wrap)
{
	m.pos = 2;
	cfg.wrap_scan = 0;
	menus_search_reset(m.state, 1, 1);
	assert_true(menus_search("a", &m, 1));
	assert_int_equal(0, m.pos);
}

TEST(backward_found_wrap)
{
	cfg.wrap_scan = 1;
	menus_search_reset(m.state, 1, 1);
	assert_true(menus_search("c", &m, 1));
	assert_int_equal(2, m.pos);
}

TEST(backward_not_found_no_wrap)
{
	cfg.wrap_scan = 0;
	menus_search_reset(m.state, 1, 1);
	assert_true(menus_search("d", &m, 1));
	assert_int_equal(0, m.pos);
}

TEST(backward_not_found_wrap)
{
	cfg.wrap_scan = 1;
	menus_search_reset(m.state, 1, 1);
	assert_true(menus_search("d", &m, 1));
	assert_int_equal(0, m.pos);
}

TEST(backward_find_next_no_wrap)
{
	m.pos = 2;
	cfg.wrap_scan = 0;
	menus_search_reset(m.state, 1, 1);
	assert_true(menus_search(".", &m, 1));
	assert_int_equal(1, m.pos);
	assert_true(menus_search(".", &m, 1));
	assert_int_equal(0, m.pos);
	assert_true(menus_search(".", &m, 1));
	assert_int_equal(0, m.pos);
}

TEST(backward_find_next_wrap)
{
	m.pos = 2;
	cfg.wrap_scan = 1;
	menus_search_reset(m.state, 1, 1);
	assert_true(menus_search(".", &m, 1));
	assert_int_equal(1, m.pos);
	assert_true(menus_search(".", &m, 1));
	assert_int_equal(0, m.pos);
	assert_true(menus_search(".", &m, 1));
	assert_int_equal(2, m.pos);
}

TEST(null_pattern_causes_pattern_reuse)
{
	menus_search_reset(m.state, 0, 1);
	assert_true(menus_search(".", &m, 1));
	assert_int_equal(1, m.pos);
	assert_true(menus_search(NULL, &m, 1));
	assert_int_equal(2, m.pos);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
