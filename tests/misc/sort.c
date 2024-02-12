#include <stic.h>

#include <unistd.h> /* chdir() unlink() */

#include <locale.h> /* LC_ALL setlocale() */
#include <stdarg.h> /* va_list va_arg() va_copy() va_end() va_start() */
#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/str.h"
#include "../../src/sort.h"
#include "../../src/status.h"

#define SIGN(n) ({__typeof(n) _n = (n); (_n < 0) ? -1 : (_n > 0);})
#define ASSERT_STRCMP_EQUAL(a, b) \
		do { assert_int_equal(SIGN(a), SIGN(b)); } while(0)

static void set_file_list(view_t *view, FileType def_ftype, ...);
SETUP_ONCE()
{
	(void)setlocale(LC_ALL, "");
}

SETUP()
{
	cfg.sort_numbers = 1;

	view_setup(&lwin);
	set_file_list(&lwin, FT_REG, "a", "_", "A", NULL);

	view_setup(&rwin);
	set_file_list(&rwin, FT_REG, "аааааааааа", "АААААААААА", NULL);

	update_string(&cfg.shell, "");
}

TEARDOWN()
{
	update_string(&cfg.shell, NULL);

	view_teardown(&lwin);
	view_teardown(&rwin);
}

TEST(special_chars_ignore_case_sort)
{
	view_set_sort(lwin.sort, SK_BY_INAME, SK_NONE);
	sort_view(&lwin);

	assert_string_equal("_", lwin.dir_entry[0].name);
}

TEST(symlink_to_dir, IF(not_windows))
{
	assert_success(chdir(SANDBOX_PATH));
	assert_success(make_symlink(".", "self"));

	view_set_sort(lwin.sort, SK_BY_INAME, SK_NONE);

	strcpy(lwin.curr_dir, SANDBOX_PATH);
	replace_string(&lwin.dir_entry[2].name, "self");
	lwin.dir_entry[2].type = FT_LINK;
	lwin.dir_entry[2].origin = lwin.curr_dir;
	lwin.dir_entry[2].dir_link = 1;

	cfg.slow_fs_list = strdup("");

	sort_view(&lwin);

	free(cfg.slow_fs_list);
	cfg.slow_fs_list = NULL;

	assert_string_equal("self", lwin.dir_entry[0].name);

	assert_int_equal(0, unlink("self"));
	assert_int_equal(0, chdir("../.."));
}

TEST(versort_without_numbers)
{
	const char *s, *t;

	s = "abc";
	t = "abcdef";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abcdef";
	t = "abc";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "";
	t = "abc";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abc";
	t = "";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "";
	t = "";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abcdef";
	t = "abcdef";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abcdef";
	t = "abcdeg";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abcdeg";
	t = "abcdef";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));
}

TEST(versort_with_numbers)
{
	const char *s, *t;

	s = "abcdef0";
	t = "abcdef0";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abcdef0";
	t = "abcdef1";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abcdef1";
	t = "abcdef0";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abcdef9";
	t = "abcdef10";
	assert_true(strnumcmp(s, t) < 0);

	s = "abcdef10";
	t = "abcdef10";
	assert_true(strnumcmp(s, t) == 0);

	s = "abcdef10";
	t = "abcdef9";
	assert_true(strnumcmp(s, t) > 0);

	s = "abcdef1.20.0";
	t = "abcdef1.5.1";
	assert_true(strnumcmp(s, t) > 0);

	s = "x001";
	t = "x1";
	assert_true(strnumcmp(s, t) < 0);

	s = "x1";
	t = "x001";
	assert_true(strnumcmp(s, t) > 0);
}

TEST(versort_numbers_only)
{
	const char *s, *t;

	s = "00";
	t = "01";
	assert_true(strnumcmp(s, t) < 0);

	s = "01";
	t = "10";
	assert_true(strnumcmp(s, t) < 0);

	s = "10";
	t = "11";
	assert_true(strnumcmp(s, t) < 0);

	s = "01";
	t = "00";
	assert_true(strnumcmp(s, t) > 0);

	s = "10";
	t = "01";
	assert_true(strnumcmp(s, t) > 0);

	s = "11";
	t = "10";
	assert_true(strnumcmp(s, t) > 0);

	s = "11";
	t = "10";
	assert_true(strnumcmp(s, t) > 0);

	s = "13";
	t = "100";
	assert_true(strnumcmp(s, t) < 0);

	s = "100";
	t = "13";
	assert_true(strnumcmp(s, t) > 0);

	s = "09";
	t = "3";
	assert_true(strnumcmp(s, t) > 0);

	s = "3";
	t = "10";
	assert_true(strnumcmp(s, t) < 0);
}

TEST(versort_numbers_only_and_letters_only)
{
	const char *s, *t;

	s = "A";
	t = "10";
	assert_true(strnumcmp(s, t) > 0);

	s = "10";
	t = "A";
	assert_true(strnumcmp(s, t) < 0);
}

TEST(versort_zero_and_zerox)
{
	const char *s, *t;

	s = "0";
	t = "01";
	assert_true(strnumcmp(s, t) < 0);
}

TEST(versort_zerox_and_one)
{
	const char *s, *t;

	s = "00_";
	t = "01";
	assert_true(strnumcmp(s, t) < 0);
}

TEST(versort_man_page_example)
{
	/* According to the `man strverscmp`:
	 *   000 < 00 < 01 < 010 < 09 < 0 < 1 < 9 < 10.
	 * In Vifm these conditions differ:
	 *  - 000 == 00;
	 *  - 010 == 09;
	 *  - 09  == 0.
	 */

	assert_true(strnumcmp("000", "00") == 0);
	assert_true(strnumcmp("00", "01") < 0);
	assert_true(strnumcmp("01", "010") < 0);
	assert_true(strnumcmp("010", "09") > 0);
	assert_true(strnumcmp("09", "0") > 0);
	assert_true(strnumcmp("0", "1") < 0);
	assert_true(strnumcmp("1", "9") < 0);
	assert_true(strnumcmp("9", "10") < 0);
}

TEST(ignore_case_name_sort_breaks_ties_deterministically)
{
	/* If normalized names are equal, byte-by-byte comparison should be used to
	 * break ties. */

	view_set_sort(rwin.sort, SK_BY_INAME, SK_NONE);
	sort_view(&rwin);

	assert_string_equal("АААААААААА", rwin.dir_entry[0].name);
	assert_string_equal("аааааааааа", rwin.dir_entry[1].name);
}

TEST(extensions_of_dot_files_are_sorted_correctly)
{
	view_teardown(&lwin);
	view_setup(&lwin);

	set_file_list(&lwin, FT_REG, "disown.c", ".cdargsresult", ".tmux.conf", NULL);
	view_set_sort(lwin.sort, SK_BY_EXTENSION, SK_NONE);
	sort_view(&lwin);

	assert_string_equal(".cdargsresult", lwin.dir_entry[0].name);
	assert_string_equal("disown.c", lwin.dir_entry[1].name);
	assert_string_equal(".tmux.conf", lwin.dir_entry[2].name);
}

TEST(sorting_has_no_integer_overflows)
{
	view_teardown(&lwin);
	view_setup(&lwin);

	set_file_list(&lwin, FT_REG, "\xff", "\x81", "\x01", "\x80", "\x7f", NULL);
	view_set_sort(lwin.sort, SK_BY_NAME, SK_NONE);
	sort_view(&lwin);

	assert_string_equal("\x01", lwin.dir_entry[0].name);
	assert_string_equal("\x7f", lwin.dir_entry[1].name);
	assert_string_equal("\x80", lwin.dir_entry[2].name);
	assert_string_equal("\x81", lwin.dir_entry[3].name);
	assert_string_equal("\xff", lwin.dir_entry[4].name);
}

TEST(sorting_uses_dcache_for_dirs)
{
	view_teardown(&lwin);
	view_setup(&lwin);
	assert_success(stats_init(&cfg));

	strcpy(lwin.curr_dir, TEST_DATA_PATH);
	set_file_list(&lwin, FT_DIR, "read", "rename", NULL);

	view_set_sort(lwin.sort, SK_BY_SIZE, SK_NONE);

	assert_success(dcache_set_at(TEST_DATA_PATH "/read", 1, 10, DCACHE_UNKNOWN));
	assert_success(dcache_set_at(TEST_DATA_PATH "/rename", 2, 100,
				DCACHE_UNKNOWN));

	sort_view(&lwin);

	assert_string_equal("read", lwin.dir_entry[0].name);
	assert_string_equal("rename", lwin.dir_entry[1].name);

	assert_success(dcache_set_at(TEST_DATA_PATH "/rename", 2, 10,
				DCACHE_UNKNOWN));
	assert_success(dcache_set_at(TEST_DATA_PATH "/read", 1, 100, DCACHE_UNKNOWN));

	sort_view(&lwin);

	assert_string_equal("rename", lwin.dir_entry[0].name);
	assert_string_equal("read", lwin.dir_entry[1].name);
}

TEST(nitems_sorting_works)
{
	view_teardown(&lwin);
	view_setup(&lwin);
	assert_success(stats_init(&cfg));

	strcpy(lwin.curr_dir, TEST_DATA_PATH);
	set_file_list(&lwin, FT_DIR, "read", "rename", "various-sizes", NULL);

	view_set_sort(lwin.sort, SK_BY_NITEMS, SK_NONE);
	sort_view(&lwin);

	assert_string_equal("rename", lwin.dir_entry[0].name);
	assert_string_equal("read", lwin.dir_entry[1].name);
	assert_string_equal("various-sizes", lwin.dir_entry[2].name);
}

TEST(groups_sorting_works)
{
	view_teardown(&lwin);
	view_setup(&lwin);
	assert_success(stats_init(&cfg));

	strcpy(lwin.curr_dir, TEST_DATA_PATH);
	set_file_list(&lwin, FT_REG, "1-done", "10-bla-todo-edit", "11-todo-publish",
			"2-todo-replace", "3-done", "4-todo-edit", "5-todo-publish", NULL);

	update_string(&lwin.sort_groups, "-(done|todo).*");
	if(lwin.primary_group_set)
	{
		regfree(&lwin.primary_group);
	}
	(void)regcomp(&lwin.primary_group, "-(done|todo).*",
			REG_EXTENDED | REG_ICASE);
	lwin.primary_group_set = 1;

	/* Ascending sorting. */

	view_set_sort(lwin.sort, SK_BY_GROUPS, SK_BY_NAME);
	sort_view(&lwin);

	assert_string_equal("1-done", lwin.dir_entry[0].name);
	assert_string_equal("3-done", lwin.dir_entry[1].name);
	assert_string_equal("2-todo-replace", lwin.dir_entry[2].name);
	assert_string_equal("4-todo-edit", lwin.dir_entry[3].name);
	assert_string_equal("5-todo-publish", lwin.dir_entry[4].name);
	assert_string_equal("10-bla-todo-edit", lwin.dir_entry[5].name);
	assert_string_equal("11-todo-publish", lwin.dir_entry[6].name);

	/* Descending sorting. */

	view_set_sort(lwin.sort, -SK_BY_GROUPS, SK_BY_NAME);
	sort_view(&lwin);

	assert_string_equal("2-todo-replace", lwin.dir_entry[0].name);
	assert_string_equal("4-todo-edit", lwin.dir_entry[1].name);
	assert_string_equal("5-todo-publish", lwin.dir_entry[2].name);
	assert_string_equal("10-bla-todo-edit", lwin.dir_entry[3].name);
	assert_string_equal("11-todo-publish", lwin.dir_entry[4].name);
	assert_string_equal("1-done", lwin.dir_entry[5].name);
	assert_string_equal("3-done", lwin.dir_entry[6].name);
}

TEST(global_groups_sorts_entries_list)
{
	update_string(&lwin.sort_groups_g, "([0-9])");
	if(lwin.primary_group_set)
	{
		regfree(&lwin.primary_group);
	}
	(void)regcomp(&lwin.primary_group, "([a-z])", REG_EXTENDED | REG_ICASE);
	lwin.primary_group_set = 1;

	dir_entry_t entry_list[] = { { .name = "a1" }, { .name = "b0" } };
	entries_t entries = { entry_list, 2 };

	view_set_sort(lwin.sort_g, SK_BY_GROUPS, SK_BY_NAME);
	sort_entries(&lwin, entries);

	assert_int_equal(2, entries.nentries);
	assert_string_equal("b0", entries.entries[0].name);
	assert_string_equal("a1", entries.entries[1].name);
}

#ifndef _WIN32

TEST(inode_sorting_works)
{
	view_teardown(&lwin);
	view_setup(&lwin);
	assert_success(stats_init(&cfg));

	strcpy(lwin.curr_dir, TEST_DATA_PATH);
	set_file_list(&lwin, FT_REG, "read", "rename", "various-sizes", NULL);
	lwin.dir_entry[0].inode = 10;
	lwin.dir_entry[1].inode = 5;
	lwin.dir_entry[2].inode = 7;

	view_set_sort(lwin.sort, SK_BY_INODE, SK_NONE);
	sort_view(&lwin);

	assert_string_equal("rename", lwin.dir_entry[0].name);
	assert_string_equal("various-sizes", lwin.dir_entry[1].name);
	assert_string_equal("read", lwin.dir_entry[2].name);
}

#endif

static void
set_file_list(view_t *view, FileType def_ftype, ...)
{
	va_list ap;
	va_start(ap, def_ftype);

	va_list aq;
	va_copy(aq, ap);

	int count = 0;
	while(va_arg(ap, const char *) != NULL)
	{
		++count;
	}
	va_end(ap);

	view->list_rows = count;
	view->dir_entry = dynarray_cextend(NULL,
			view->list_rows*sizeof(*view->dir_entry));

	int i;
	for(i = 0; i < count; ++i)
	{
		view->dir_entry[i].name = strdup(va_arg(aq, const char *));
		view->dir_entry[i].type = def_ftype;
		view->dir_entry[i].origin = view->curr_dir;
#ifndef _WIN32
		view->dir_entry[i].inode = 1 + i;
#endif
	}
	va_end(aq);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
