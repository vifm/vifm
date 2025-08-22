#include <stic.h>

#include <stddef.h> /* NULL */
#include <string.h> /* strlen() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/os.h"
#include "../../src/ui/ui.h"
#include "../../src/marks.h"

static void suggest_cb_no_call(const wchar_t text[], const wchar_t value[],
		const char descr[]);
static void suggest_cb_a_lpath(const wchar_t text[], const wchar_t value[],
		const char descr[]);
static void suggest_cb_a_testdir(const wchar_t text[], const wchar_t value[],
		const char descr[]);
static void suggest_cb_a_testdir_slash(const wchar_t text[],
		const wchar_t value[], const char descr[]);

static int calls;

SETUP()
{
	cfg.slow_fs_list = strdup("");
	lwin.list_pos = 0;
	lwin.column_count = 1;
	rwin.list_pos = 0;
	rwin.column_count = 1;

	calls = 0;
}

TEARDOWN()
{
	free(cfg.slow_fs_list);
	cfg.slow_fs_list = NULL;

	marks_clear_all();
}

TEST(unexistent_mark)
{
	assert_int_equal(1, marks_goto(&lwin, 'b'));
}

TEST(all_valid_marks_can_be_queried)
{
	const int bookmark_count = strlen(marks_all);
	int i;
	for(i = 0; i < bookmark_count; ++i)
	{
		assert_true(marks_by_index(&lwin, i) != NULL);
	}
}

TEST(regular_marks_are_global)
{
	char c;
	for(c = 'a'; c <= 'z'; ++c)
	{
		const mark_t *mark;

		marks_set_user(&lwin, c, "lpath", "lfile");
		marks_set_user(&rwin, c, "rpath", "rfile");

		mark = get_mark_by_name(&lwin, c);
		assert_string_equal("rpath", mark->directory);
		assert_string_equal("rfile", mark->file);

		mark = get_mark_by_name(&rwin, c);
		assert_string_equal("rpath", mark->directory);
		assert_string_equal("rfile", mark->file);
	}
}

TEST(sel_marks_are_local)
{
	const mark_t *mark;

	marks_set_special(&lwin, '<', "lpath", "lfile<");
	marks_set_special(&lwin, '>', "lpath", "lfile>");

	marks_set_special(&rwin, '<', "rpath", "rfile<");
	marks_set_special(&rwin, '>', "rpath", "rfile>");

	mark = get_mark_by_name(&lwin, '<');
	assert_string_equal("lpath", mark->directory);
	assert_string_equal("lfile<", mark->file);
	mark = get_mark_by_name(&lwin, '>');
	assert_string_equal("lpath", mark->directory);
	assert_string_equal("lfile>", mark->file);

	mark = get_mark_by_name(&rwin, '<');
	assert_string_equal("rpath", mark->directory);
	assert_string_equal("rfile<", mark->file);
	mark = get_mark_by_name(&rwin, '>');
	assert_string_equal("rpath", mark->directory);
	assert_string_equal("rfile>", mark->file);
}

TEST(marks_are_suggested)
{
	view_setup(&lwin);

	/* No marks to suggest. */
	marks_suggest(&lwin, &suggest_cb_no_call, /*local_only=*/0);
	assert_int_equal(0, calls);

	/* Adding one mark. */
	marks_set_user(&lwin, 'a', "lpath", "lfile");

	/* Nothing will be suggested because the only mark isn't local. */
	marks_suggest(&lwin, &suggest_cb_a_lpath, /*local_only=*/1);
	assert_int_equal(0, calls);

	/* All marks are suggested and there is only one. */
	marks_suggest(&lwin, &suggest_cb_a_lpath, /*local_only=*/0);
	assert_int_equal(1, calls);

	/* Make the mark local. */
	strcpy(lwin.curr_dir, "lpath");
	append_view_entry(&lwin, "lfile");

	/* Now the mark is suggested as local. */
	marks_suggest(&lwin, &suggest_cb_a_lpath, /*local_only=*/1);
	assert_int_equal(2, calls);

	/* And among all marks as well. */
	marks_suggest(&lwin, &suggest_cb_a_lpath, /*local_only=*/0);
	assert_int_equal(3, calls);

	view_teardown(&lwin);
}

TEST(marks_are_suggested_with_trailing_slash)
{
	assert_success(os_chdir(SANDBOX_PATH));
	create_dir("testdir");

	marks_set_user(&lwin, 'a', "testdir", "somefile");
	marks_suggest(&lwin, &suggest_cb_a_testdir, /*local_only=*/0);
	assert_int_equal(1, calls);

	marks_set_user(&lwin, 'a', "testdir", "..");
	marks_suggest(&lwin, &suggest_cb_a_testdir_slash, /*local_only=*/0);
	assert_int_equal(2, calls);

	remove_dir("testdir");
}

static void
suggest_cb_no_call(const wchar_t text[], const wchar_t value[],
		const char descr[])
{
	assert_fail("Unexpected invocation of marks' callback.");
	++calls;
}

static void
suggest_cb_a_lpath(const wchar_t text[], const wchar_t value[],
		const char descr[])
{
	assert_wstring_equal(L"mark: a", text);
	assert_wstring_equal(L"", value);
	assert_string_equal("lpath", descr);
	++calls;
}

static void
suggest_cb_a_testdir(const wchar_t text[], const wchar_t value[],
		const char descr[])
{
	assert_wstring_equal(L"mark: a", text);
	assert_wstring_equal(L"", value);
	assert_string_equal("testdir/somefile", descr);
	++calls;
}

static void
suggest_cb_a_testdir_slash(const wchar_t text[], const wchar_t value[],
		const char descr[])
{
	assert_wstring_equal(L"mark: a", text);
	assert_wstring_equal(L"", value);
	assert_string_equal("testdir/", descr);
	++calls;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
