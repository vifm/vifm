#include <stic.h>

#include <unistd.h> /* chdir() symlink() */

#include <stdlib.h> /* remove() */
#include <string.h> /* strcpy() strdup() */

#include "../../src/cfg/config.h"
#include "../../src/menus/menus.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/string_array.h"

#include "utils.h"

static menu_data_t m;

SETUP()
{
	init_menu_data(&m, &lwin, strdup("test"), strdup("No matches"));

	m.len = add_to_string_array(&m.items, m.len, 1, "a");
	m.len = add_to_string_array(&m.items, m.len, 1, "b");
	m.len = add_to_string_array(&m.items, m.len, 1, "c");
}

TEARDOWN()
{
	reset_menu_data(&m);
}

TEST(can_navigate_to_broken_symlink, IF(not_windows))
{
	char *saved_cwd;

	strcpy(lwin.curr_dir, ".");

	saved_cwd = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	/* symlink() is not available on Windows, but other code is fine. */
#ifndef _WIN32
	assert_success(symlink("/wrong/path", "broken-link"));
#endif

	/* Were trying to open broken link, which will fail, but the parsing part
	 * should succeed. */
	restore_cwd(saved_cwd);
	assert_success(goto_selected_file(&m, &lwin, SANDBOX_PATH "/broken-link:",
				1));

	assert_success(remove(SANDBOX_PATH "/broken-link"));
}

TEST(nothing_is_searched_if_no_pattern)
{
	menus_search(m.state, 0);
	assert_int_equal(0, menu_get_matches(m.state));
}

TEST(nothing_is_searched_for_wrong_pattern)
{
	menu_new_search(m.state, 0, 1);
	assert_true(search_menu_list("*a", &m, 1));
	assert_int_equal(0, menu_get_matches(m.state));
}

TEST(search_via_menu_search)
{
	menu_new_search(m.state, 0, 1);
	assert_true(search_menu_list("[abc]", &m, 1));
	assert_int_equal(1, m.pos);
	menus_search(m.state, 0);
	assert_int_equal(2, m.pos);
}

TEST(ok_to_print_message_if_there_is_no_pattern)
{
	menu_print_search_msg(m.state);
}

TEST(ok_to_print_message_for_wrong_pattern)
{
	assert_true(search_menu_list("*", &m, 1));
	menu_print_search_msg(m.state);
}

TEST(forward_found_no_wrap)
{
	cfg.wrap_scan = 0;
	assert_true(search_menu_list("c", &m, 1));
	assert_int_equal(2, m.pos);
}

TEST(forward_found_wrap)
{
	m.pos = 1;
	cfg.wrap_scan = 1;
	assert_true(search_menu_list("a", &m, 1));
	assert_int_equal(0, m.pos);
}

TEST(forward_not_found_no_wrap)
{
	cfg.wrap_scan = 0;
	assert_true(search_menu_list("d", &m, 1));
	assert_int_equal(0, m.pos);
}

TEST(forward_not_found_wrap)
{
	cfg.wrap_scan = 1;
	assert_true(search_menu_list("d", &m, 1));
	assert_int_equal(0, m.pos);
}

TEST(forward_find_next_no_wrap)
{
	cfg.wrap_scan = 0;
	assert_true(search_menu_list(".", &m, 1));
	assert_int_equal(1, m.pos);
	assert_true(search_menu_list(".", &m, 1));
	assert_int_equal(2, m.pos);
	assert_true(search_menu_list(".", &m, 1));
	assert_int_equal(2, m.pos);
}

TEST(forward_find_next_wrap)
{
	cfg.wrap_scan = 1;
	assert_true(search_menu_list(".", &m, 1));
	assert_int_equal(1, m.pos);
	assert_true(search_menu_list(".", &m, 1));
	assert_int_equal(2, m.pos);
	assert_true(search_menu_list(".", &m, 1));
	assert_int_equal(0, m.pos);
}

TEST(backward_found_no_wrap)
{
	m.pos = 2;
	cfg.wrap_scan = 0;
	menu_new_search(m.state, 1, 1);
	assert_true(search_menu_list("a", &m, 1));
	assert_int_equal(0, m.pos);
}

TEST(backward_found_wrap)
{
	cfg.wrap_scan = 1;
	menu_new_search(m.state, 1, 1);
	assert_true(search_menu_list("c", &m, 1));
	assert_int_equal(2, m.pos);
}

TEST(backward_not_found_no_wrap)
{
	cfg.wrap_scan = 0;
	menu_new_search(m.state, 1, 1);
	assert_true(search_menu_list("d", &m, 1));
	assert_int_equal(0, m.pos);
}

TEST(backward_not_found_wrap)
{
	cfg.wrap_scan = 1;
	menu_new_search(m.state, 1, 1);
	assert_true(search_menu_list("d", &m, 1));
	assert_int_equal(0, m.pos);
}

TEST(backward_find_next_no_wrap)
{
	m.pos = 2;
	cfg.wrap_scan = 0;
	menu_new_search(m.state, 1, 1);
	assert_true(search_menu_list(".", &m, 1));
	assert_int_equal(1, m.pos);
	assert_true(search_menu_list(".", &m, 1));
	assert_int_equal(0, m.pos);
	assert_true(search_menu_list(".", &m, 1));
	assert_int_equal(0, m.pos);
}

TEST(backward_find_next_wrap)
{
	m.pos = 2;
	cfg.wrap_scan = 1;
	menu_new_search(m.state, 1, 1);
	assert_true(search_menu_list(".", &m, 1));
	assert_int_equal(1, m.pos);
	assert_true(search_menu_list(".", &m, 1));
	assert_int_equal(0, m.pos);
	assert_true(search_menu_list(".", &m, 1));
	assert_int_equal(2, m.pos);
}

TEST(null_pattern_causes_pattern_reuse)
{
	menu_new_search(m.state, 0, 1);
	assert_true(search_menu_list(".", &m, 1));
	assert_int_equal(1, m.pos);
	assert_true(search_menu_list(NULL, &m, 1));
	assert_int_equal(2, m.pos);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
