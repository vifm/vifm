#include <stic.h>

#include <sys/stat.h> /* chmod() */
#include <unistd.h> /* rmdir() */

#include <stdio.h> /* FILE fopen() fwrite() fclose() remove() */
#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/ui/ui.h"
#include "../../src/compare.h"

/* These tests are about comparison strategies and not about handling of unusual
 * situations or results of operations in compare views. */

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;

	view_setup(&lwin);
	view_setup(&rwin);

	opt_handlers_setup();

	columns_setup_column(SK_BY_NAME);
	columns_setup_column(SK_BY_SIZE);
}

TEARDOWN()
{
	columns_teardown();

	view_teardown(&lwin);
	view_teardown(&rwin);

	opt_handlers_teardown();
}

TEST(files_are_compared_by_name)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/a");
	compare_one_pane(&lwin, CT_NAME, LT_ALL, CF_NONE);

	assert_int_equal(CV_COMPARE, lwin.custom.type);
	assert_int_equal(3, lwin.list_rows);
	assert_int_equal(1, lwin.dir_entry[0].id);
	assert_int_equal(2, lwin.dir_entry[1].id);
	assert_int_equal(3, lwin.dir_entry[2].id);
}

TEST(files_are_compared_by_size)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/a");
	compare_one_pane(&lwin, CT_SIZE, LT_ALL, CF_NONE);

	assert_int_equal(CV_COMPARE, lwin.custom.type);
	assert_int_equal(3, lwin.list_rows);
	assert_int_equal(1, lwin.dir_entry[0].id);
	assert_int_equal(2, lwin.dir_entry[1].id);
	assert_int_equal(3, lwin.dir_entry[2].id);
}

TEST(files_are_compared_by_contents)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/b");
	compare_one_pane(&lwin, CT_CONTENTS, LT_ALL, CF_NONE);

	assert_int_equal(CV_COMPARE, lwin.custom.type);
	assert_int_equal(4, lwin.list_rows);
	assert_int_equal(1, lwin.dir_entry[0].id);
	assert_int_equal(1, lwin.dir_entry[1].id);
	assert_int_equal(2, lwin.dir_entry[2].id);
	assert_int_equal(3, lwin.dir_entry[3].id);
}

TEST(two_panes_by_name_ignore_case)
{
	create_file(SANDBOX_PATH "/A");
	create_file(SANDBOX_PATH "/Aa");
	create_file(SANDBOX_PATH "/aAa");

	strcpy(lwin.curr_dir, SANDBOX_PATH);
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/rename");

	compare_two_panes(CT_NAME, LT_ALL, CF_SHOW | CF_GROUP_PATHS | CF_IGNORE_CASE);

	check_compare_invariants(3);

	assert_int_equal(1, lwin.dir_entry[0].id);
	assert_int_equal(2, lwin.dir_entry[1].id);
	assert_int_equal(3, lwin.dir_entry[2].id);

	assert_string_equal("A", lwin.dir_entry[0].name);
	assert_string_equal("a", rwin.dir_entry[0].name);
	assert_string_equal("Aa", lwin.dir_entry[1].name);
	assert_string_equal("aa", rwin.dir_entry[1].name);
	assert_string_equal("aAa", lwin.dir_entry[2].name);
	assert_string_equal("aaa", rwin.dir_entry[2].name);

	remove_file(SANDBOX_PATH "/A");
	remove_file(SANDBOX_PATH "/Aa");
	remove_file(SANDBOX_PATH "/aAa");
}

TEST(two_panes_by_name_ignore_case_sorts_with_isort)
{
	create_dir(SANDBOX_PATH "/l");
	create_file(SANDBOX_PATH "/l/a");
	create_file(SANDBOX_PATH "/l/b");
	create_dir(SANDBOX_PATH "/r");
	create_file(SANDBOX_PATH "/r/a");
	create_file(SANDBOX_PATH "/r/B");

	strcpy(lwin.curr_dir, SANDBOX_PATH "/l");
	strcpy(rwin.curr_dir, SANDBOX_PATH "/r");

	compare_two_panes(CT_NAME, LT_ALL, CF_SHOW | CF_GROUP_PATHS | CF_IGNORE_CASE);

	check_compare_invariants(2);

	assert_int_equal(1, lwin.dir_entry[0].id);
	assert_int_equal(2, lwin.dir_entry[1].id);

	assert_string_equal("a", lwin.dir_entry[0].name);
	assert_string_equal("a", rwin.dir_entry[0].name);
	assert_string_equal("b", lwin.dir_entry[1].name);
	assert_string_equal("B", rwin.dir_entry[1].name);

	remove_file(SANDBOX_PATH "/l/a");
	remove_file(SANDBOX_PATH "/l/b");
	remove_dir(SANDBOX_PATH "/l");
	remove_file(SANDBOX_PATH "/r/a");
	remove_file(SANDBOX_PATH "/r/B");
	remove_dir(SANDBOX_PATH "/r");
}

TEST(two_panes_by_name_respect_case)
{
	create_file(SANDBOX_PATH "/A");
	create_file(SANDBOX_PATH "/Aa");
	create_file(SANDBOX_PATH "/aAa");

	strcpy(lwin.curr_dir, SANDBOX_PATH);
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/rename");

	compare_two_panes(CT_NAME, LT_ALL,
			CF_SHOW | CF_GROUP_PATHS | CF_RESPECT_CASE);

	check_compare_invariants(6);

	remove_file(SANDBOX_PATH "/A");
	remove_file(SANDBOX_PATH "/Aa");
	remove_file(SANDBOX_PATH "/aAa");
}

TEST(two_panes_by_name_respace_case_sorts_with_sort)
{
	create_dir(SANDBOX_PATH "/l");
	create_file(SANDBOX_PATH "/l/a");
	create_file(SANDBOX_PATH "/l/b");
	create_dir(SANDBOX_PATH "/r");
	create_file(SANDBOX_PATH "/r/a");
	create_file(SANDBOX_PATH "/r/B");

	strcpy(lwin.curr_dir, SANDBOX_PATH "/l");
	strcpy(rwin.curr_dir, SANDBOX_PATH "/r");

	compare_two_panes(CT_NAME, LT_ALL,
			CF_SHOW | CF_GROUP_PATHS | CF_RESPECT_CASE);

	check_compare_invariants(3);

	assert_int_equal(3, lwin.dir_entry[0].id);
	assert_int_equal(1, lwin.dir_entry[1].id);
	assert_int_equal(2, lwin.dir_entry[2].id);

	assert_string_equal("", lwin.dir_entry[0].name);
	assert_string_equal("B", rwin.dir_entry[0].name);
	assert_string_equal("a", lwin.dir_entry[1].name);
	assert_string_equal("a", rwin.dir_entry[1].name);
	assert_string_equal("b", lwin.dir_entry[2].name);
	assert_string_equal("", rwin.dir_entry[2].name);

	remove_file(SANDBOX_PATH "/l/a");
	remove_file(SANDBOX_PATH "/l/b");
	remove_dir(SANDBOX_PATH "/l");
	remove_file(SANDBOX_PATH "/r/a");
	remove_file(SANDBOX_PATH "/r/B");
	remove_dir(SANDBOX_PATH "/r");
}

TEST(two_panes_all_group_ids)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/a");
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/compare/b");
	compare_two_panes(CT_NAME, LT_ALL, CF_SHOW);

	check_compare_invariants(4);

	assert_int_equal(1, lwin.dir_entry[0].id);
	assert_int_equal(2, lwin.dir_entry[1].id);
	assert_int_equal(3, lwin.dir_entry[2].id);
	assert_int_equal(4, lwin.dir_entry[3].id);

	assert_string_equal("same-content-different-name-1", lwin.dir_entry[0].name);
	assert_string_equal("same-content-different-name-1", rwin.dir_entry[0].name);
	assert_string_equal("same-name-different-content", lwin.dir_entry[1].name);
	assert_string_equal("same-name-different-content", rwin.dir_entry[1].name);
	assert_string_equal("same-name-same-content", lwin.dir_entry[2].name);
	assert_string_equal("same-name-same-content", rwin.dir_entry[2].name);
	assert_string_equal("", lwin.dir_entry[3].name);
	assert_string_equal("same-content-different-name-2", rwin.dir_entry[3].name);
}

TEST(two_panes_all_group_paths)
{
	curr_view = &rwin;
	other_view = &lwin;
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/a");
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/compare/b");
	compare_two_panes(CT_NAME, LT_ALL, CF_GROUP_PATHS | CF_SHOW);

	check_compare_invariants(4);

	assert_int_equal(1, lwin.dir_entry[0].id);
	assert_int_equal(2, lwin.dir_entry[1].id);
	assert_int_equal(3, lwin.dir_entry[2].id);
	assert_int_equal(4, lwin.dir_entry[3].id);

	assert_string_equal("same-content-different-name-1", lwin.dir_entry[0].name);
	assert_string_equal("same-content-different-name-1", rwin.dir_entry[0].name);
	assert_string_equal("", lwin.dir_entry[1].name);
	assert_string_equal("same-content-different-name-2", rwin.dir_entry[1].name);
	assert_string_equal("same-name-different-content", lwin.dir_entry[2].name);
	assert_string_equal("same-name-different-content", rwin.dir_entry[2].name);
	assert_string_equal("same-name-same-content", lwin.dir_entry[3].name);
	assert_string_equal("same-name-same-content", rwin.dir_entry[3].name);
}

TEST(two_panes_dups_one_is_empty)
{
	curr_view = &rwin;
	other_view = &lwin;
	strcpy(lwin.curr_dir, SANDBOX_PATH);
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/compare/b");
	compare_two_panes(CT_CONTENTS, LT_ALL, CF_GROUP_PATHS | CF_SHOW);

	check_compare_invariants(4);

	assert_int_equal(1, lwin.dir_entry[0].id);
	assert_int_equal(1, lwin.dir_entry[1].id);
	assert_int_equal(2, lwin.dir_entry[2].id);
	assert_int_equal(3, lwin.dir_entry[3].id);

	assert_string_equal("", lwin.dir_entry[0].name);
	assert_string_equal("same-content-different-name-1", rwin.dir_entry[0].name);
	assert_string_equal("", lwin.dir_entry[1].name);
	assert_string_equal("same-content-different-name-2", rwin.dir_entry[1].name);
	assert_string_equal("", lwin.dir_entry[2].name);
	assert_string_equal("same-name-different-content", rwin.dir_entry[2].name);
	assert_string_equal("", lwin.dir_entry[3].name);
	assert_string_equal("same-name-same-content", rwin.dir_entry[3].name);
}

TEST(two_panes_dups)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/a");
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/compare/b");
	compare_two_panes(CT_CONTENTS, LT_DUPS, CF_GROUP_PATHS | CF_SHOW);

	check_compare_invariants(3);

	assert_int_equal(1, lwin.dir_entry[0].id);
	assert_int_equal(1, lwin.dir_entry[1].id);
	assert_int_equal(2, lwin.dir_entry[2].id);

	assert_string_equal("same-content-different-name-1", lwin.dir_entry[0].name);
	assert_string_equal("same-content-different-name-1", rwin.dir_entry[0].name);
	assert_string_equal("", lwin.dir_entry[1].name);
	assert_string_equal("same-content-different-name-2", rwin.dir_entry[1].name);
	assert_string_equal("same-name-same-content", lwin.dir_entry[2].name);
	assert_string_equal("same-name-same-content", rwin.dir_entry[2].name);
}

TEST(two_panes_unique)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/a");
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/compare/b");
	compare_two_panes(CT_CONTENTS, LT_UNIQUE, CF_GROUP_PATHS | CF_SHOW);

	assert_int_equal(1, lwin.list_rows);
	assert_int_equal(1, rwin.list_rows);

	assert_string_equal("same-name-different-content", lwin.dir_entry[0].name);
	assert_string_equal("same-name-different-content", rwin.dir_entry[0].name);
}

TEST(single_pane_all)
{
	copy_file(TEST_DATA_PATH "/read/dos-eof", SANDBOX_PATH "/dos-eof-1");
	copy_file(TEST_DATA_PATH "/read/dos-eof", SANDBOX_PATH "/dos-eof-2");
	copy_file(TEST_DATA_PATH "/read/utf8-bom", SANDBOX_PATH "/utf8-bom-1");
	copy_file(TEST_DATA_PATH "/read/utf8-bom", SANDBOX_PATH "/utf8-bom-2");

	strcpy(lwin.curr_dir, SANDBOX_PATH);
	compare_one_pane(&lwin, CT_CONTENTS, LT_ALL, CF_NONE);

	assert_int_equal(CV_COMPARE, lwin.custom.type);
	assert_int_equal(4, lwin.list_rows);
	assert_string_equal("dos-eof-1", lwin.dir_entry[0].name);
	assert_int_equal(1, lwin.dir_entry[0].id);
	assert_int_equal(1, lwin.dir_entry[1].id);
	assert_string_equal("utf8-bom-1", lwin.dir_entry[2].name);
	assert_int_equal(2, lwin.dir_entry[2].id);
	assert_int_equal(2, lwin.dir_entry[3].id);

	assert_success(remove(SANDBOX_PATH "/dos-eof-1"));
	assert_success(remove(SANDBOX_PATH "/dos-eof-2"));
	assert_success(remove(SANDBOX_PATH "/utf8-bom-1"));
	assert_success(remove(SANDBOX_PATH "/utf8-bom-2"));
}

TEST(single_pane_dups)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare");
	compare_one_pane(&lwin, CT_CONTENTS, LT_DUPS, CF_NONE);

	assert_int_equal(CV_COMPARE, lwin.custom.type);
	assert_int_equal(5, lwin.list_rows);
	assert_string_equal("same-content-different-name-1", lwin.dir_entry[0].name);
	assert_int_equal(1, lwin.dir_entry[0].id);
	assert_int_equal(1, lwin.dir_entry[1].id);
	assert_int_equal(1, lwin.dir_entry[2].id);
	assert_string_equal("same-name-same-content", lwin.dir_entry[3].name);
	assert_int_equal(2, lwin.dir_entry[3].id);
	assert_int_equal(2, lwin.dir_entry[4].id);
}

TEST(single_pane_unique)
{
	copy_file(TEST_DATA_PATH "/read/dos-eof", SANDBOX_PATH "/dos-eof");
	copy_file(TEST_DATA_PATH "/read/utf8-bom", SANDBOX_PATH "/utf8-bom-1");
	copy_file(TEST_DATA_PATH "/read/utf8-bom", SANDBOX_PATH "/utf8-bom-2");

	strcpy(lwin.curr_dir, SANDBOX_PATH);
	compare_one_pane(&lwin, CT_CONTENTS, LT_UNIQUE, CF_NONE);

	assert_int_equal(CV_REGULAR, lwin.custom.type);
	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("dos-eof", lwin.dir_entry[0].name);

	assert_success(remove(SANDBOX_PATH "/dos-eof"));
	assert_success(remove(SANDBOX_PATH "/utf8-bom-1"));
	assert_success(remove(SANDBOX_PATH "/utf8-bom-2"));
}

TEST(relatively_complex_match)
{
	copy_file(TEST_DATA_PATH "/read/dos-eof", SANDBOX_PATH "/dos-eof-1");
	copy_file(TEST_DATA_PATH "/read/dos-eof", SANDBOX_PATH "/dos-eof-2");
	copy_file(TEST_DATA_PATH "/read/utf8-bom", SANDBOX_PATH "/utf8-bom-1");
	copy_file(TEST_DATA_PATH "/read/utf8-bom", SANDBOX_PATH "/utf8-bom-2");

	curr_view = &rwin;
	other_view = &lwin;
	strcpy(lwin.curr_dir, SANDBOX_PATH);
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/read");
	compare_two_panes(CT_CONTENTS, LT_ALL, CF_GROUP_PATHS | CF_SHOW);

	check_compare_invariants(10);

	assert_int_equal(1, lwin.dir_entry[0].id);
	assert_int_equal(2, lwin.dir_entry[1].id);
	assert_int_equal(2, lwin.dir_entry[2].id);
	assert_int_equal(2, lwin.dir_entry[3].id);
	assert_int_equal(3, lwin.dir_entry[4].id);
	assert_int_equal(4, lwin.dir_entry[5].id);
	assert_int_equal(5, lwin.dir_entry[6].id);
	assert_int_equal(5, lwin.dir_entry[7].id);
	assert_int_equal(5, lwin.dir_entry[8].id);
	assert_int_equal(6, lwin.dir_entry[9].id);

	assert_string_equal("", lwin.dir_entry[0].name);
	assert_string_equal("binary-data", rwin.dir_entry[0].name);

	assert_string_equal("", lwin.dir_entry[1].name);
	assert_string_equal("dos-eof", rwin.dir_entry[1].name);

	assert_string_equal("dos-eof-1", lwin.dir_entry[2].name);
	assert_string_equal("", rwin.dir_entry[2].name);

	assert_string_equal("dos-eof-2", lwin.dir_entry[3].name);
	assert_string_equal("", rwin.dir_entry[3].name);

	assert_string_equal("", lwin.dir_entry[4].name);
	assert_string_equal("dos-line-endings", rwin.dir_entry[4].name);

	assert_string_equal("", lwin.dir_entry[5].name);
	assert_string_equal("two-lines", rwin.dir_entry[5].name);

	assert_string_equal("", lwin.dir_entry[6].name);
	assert_string_equal("utf8-bom", rwin.dir_entry[6].name);

	assert_string_equal("utf8-bom-1", lwin.dir_entry[7].name);
	assert_string_equal("", rwin.dir_entry[7].name);

	assert_string_equal("utf8-bom-2", lwin.dir_entry[8].name);
	assert_string_equal("", rwin.dir_entry[8].name);

	assert_string_equal("", lwin.dir_entry[9].name);
	assert_string_equal("very-long-line", rwin.dir_entry[9].name);

	assert_success(remove(SANDBOX_PATH "/dos-eof-1"));
	assert_success(remove(SANDBOX_PATH "/dos-eof-2"));
	assert_success(remove(SANDBOX_PATH "/utf8-bom-1"));
	assert_success(remove(SANDBOX_PATH "/utf8-bom-2"));
}

/* Tests hashing of files with identical size. */
TEST(content_difference_is_detected)
{
	create_dir(SANDBOX_PATH "/a");
	create_dir(SANDBOX_PATH "/b");
	copy_file(TEST_DATA_PATH "/read/two-lines", SANDBOX_PATH "/a/two-lines");
	copy_file(TEST_DATA_PATH "/read/two-lines", SANDBOX_PATH "/b/two-lines");

	FILE *fp = fopen(SANDBOX_PATH "/b/two-lines", "r+b");
	char data = 'x';
	assert_int_equal(1, fwrite(&data, 1, 1, fp));
	fclose(fp);

	curr_view = &lwin;
	other_view = &rwin;
	strcpy(lwin.curr_dir, SANDBOX_PATH "/a");
	strcpy(rwin.curr_dir, SANDBOX_PATH "/b");
	compare_two_panes(CT_CONTENTS, LT_ALL, CF_GROUP_PATHS | CF_SHOW);

	assert_int_equal(1, lwin.list_rows);
	assert_int_equal(1, rwin.list_rows);

	assert_int_equal(1, lwin.dir_entry[0].id);
	assert_int_equal(2, rwin.dir_entry[0].id);

	remove_file(SANDBOX_PATH "/a/two-lines");
	remove_file(SANDBOX_PATH "/b/two-lines");
	remove_dir(SANDBOX_PATH "/a");
	remove_dir(SANDBOX_PATH "/b");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
