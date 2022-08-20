#include <stic.h>

#include <sys/stat.h> /* chmod() */

#include <limits.h> /* INT_MAX */
#include <string.h> /* memset() strcpy() */
#include <time.h> /* time() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/ui/column_view.h"
#include "../../src/ui/fileview.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/cancellation.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/str.h"
#include "../../src/compare.h"
#include "../../src/filelist.h"
#include "../../src/flist_pos.h"
#include "../../src/fops_misc.h"
#include "../../src/status.h"

static char cwd[PATH_MAX + 1];

SETUP_ONCE()
{
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
}

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;

	view_setup(&lwin);

	opt_handlers_setup();
}

TEARDOWN()
{
	view_teardown(&lwin);

	opt_handlers_teardown();

	cfg.ignore_case = 0;
}

TEST(compare_view_defines_id_grouping)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH,
			"compare/a", cwd);
	compare_one_pane(&lwin, CT_CONTENTS, LT_ALL, 0);

	assert_int_equal(3, lwin.list_rows);

	assert_int_equal(0, lwin.list_pos);
	assert_int_equal(1, lwin.list_pos = fpos_find_group(&lwin, 1));
	assert_int_equal(2, lwin.list_pos = fpos_find_group(&lwin, 1));
	assert_int_equal(1, lwin.list_pos = fpos_find_group(&lwin, 0));
}

TEST(goto_file_nagivates_to_files)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH,
			"existing-files", cwd);
	load_dir_list(&lwin, 1);

	cfg.ignore_case = 1;

	assert_int_equal(-1, fpos_find_by_ch(&lwin, 'a', 0, 0));
	assert_int_equal(-1, fpos_find_by_ch(&lwin, 'A', 1, 0));
	assert_int_equal(0, fpos_find_by_ch(&lwin, 'A', 0, 1));
	assert_int_equal(0, fpos_find_by_ch(&lwin, 'a', 1, 1));
	assert_int_equal(1, fpos_find_by_ch(&lwin, 'b', 0, 0));
	assert_int_equal(-1, fpos_find_by_ch(&lwin, 'B', 1, 0));
	assert_int_equal(1, fpos_find_by_ch(&lwin, 'B', 0, 1));
	assert_int_equal(1, fpos_find_by_ch(&lwin, 'b', 1, 1));
}

TEST(goto_file_nagivates_to_files_with_case_override)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH,
			"existing-files", cwd);
	load_dir_list(&lwin, 1);

	cfg.ignore_case = 1;
	cfg.case_override = CO_GOTO_FILE;
	cfg.case_ignore = 0;

	assert_int_equal(-1, fpos_find_by_ch(&lwin, 'a', 0, 0));
	assert_int_equal(-1, fpos_find_by_ch(&lwin, 'A', 1, 0));
	assert_int_equal(0, fpos_find_by_ch(&lwin, 'A', 0, 1));
	assert_int_equal(0, fpos_find_by_ch(&lwin, 'a', 1, 1));
	assert_int_equal(1, fpos_find_by_ch(&lwin, 'b', 0, 0));
	assert_int_equal(-1, fpos_find_by_ch(&lwin, 'B', 1, 0));
	assert_int_equal(0, fpos_find_by_ch(&lwin, 'B', 0, 1));
	assert_int_equal(1, fpos_find_by_ch(&lwin, 'b', 1, 1));

	cfg.case_override = 0;
}

TEST(find_directory)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH,
			"tree/dir1", cwd);
	load_dir_list(&lwin, 1);

	assert_int_equal(2, lwin.list_rows);

	assert_int_equal(0, fpos_next_dir(&lwin));
	assert_int_equal(0, fpos_prev_dir(&lwin));

	lwin.list_pos = 1;

	assert_int_equal(1, fpos_next_dir(&lwin));
	assert_int_equal(0, fpos_prev_dir(&lwin));
}

TEST(find_selected)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH,
			"existing-files", cwd);
	load_dir_list(&lwin, 1);

	assert_int_equal(3, lwin.list_rows);
	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[2].selected = 1;
	lwin.selected_files = 2;

	assert_int_equal(2, fpos_next_selected(&lwin));
	assert_int_equal(0, fpos_prev_selected(&lwin));

	lwin.list_pos = 1;

	assert_int_equal(2, fpos_next_selected(&lwin));
	assert_int_equal(0, fpos_prev_selected(&lwin));
}

TEST(find_first_and_last_siblings)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "tree",
			cwd);
	assert_success(flist_load_tree(&lwin, lwin.curr_dir, INT_MAX));
	assert_int_equal(12, lwin.list_rows);

	assert_int_equal(0, fpos_first_sibling(&lwin));
	assert_int_equal(11, fpos_last_sibling(&lwin));

	lwin.list_pos = 8;

	assert_int_equal(0, fpos_first_sibling(&lwin));
	assert_int_equal(11, fpos_last_sibling(&lwin));

	lwin.list_pos = 11;
	assert_int_equal(0, fpos_first_sibling(&lwin));
	assert_int_equal(11, fpos_last_sibling(&lwin));

	lwin.list_pos = 1;
	assert_int_equal(1, fpos_first_sibling(&lwin));
	assert_int_equal(7, fpos_last_sibling(&lwin));
}

TEST(find_next_and_prev_dir_sibling)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "tree",
			cwd);
	assert_success(flist_load_tree(&lwin, lwin.curr_dir, INT_MAX));
	assert_int_equal(12, lwin.list_rows);

	assert_int_equal(0, fpos_prev_dir_sibling(&lwin));
	assert_int_equal(8, fpos_next_dir_sibling(&lwin));

	lwin.list_pos = 8;

	assert_int_equal(0, fpos_prev_dir_sibling(&lwin));
	assert_int_equal(8, fpos_next_dir_sibling(&lwin));

	lwin.list_pos = 11;

	assert_int_equal(8, fpos_prev_dir_sibling(&lwin));
	assert_int_equal(11, fpos_next_dir_sibling(&lwin));

	/* When last sibling is an empty dir. */

	cfg.dot_dirs &= ~DD_TREE_LEAFS_PARENT;
	lwin.dir_entry[6].selected = 1;
	lwin.selected_files = 1;
	flist_custom_exclude(&lwin, 1);
	assert_int_equal(11, lwin.list_rows);

	lwin.list_pos = 5;
	assert_int_equal(2, fpos_prev_dir_sibling(&lwin));
	lwin.list_pos = 2;
	assert_int_equal(5, fpos_next_dir_sibling(&lwin));
}

TEST(find_next_and_prev_mismatches)
{
	curr_view = &lwin;
	other_view = &rwin;

	view_setup(&rwin);

	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/a");
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/compare/b");
	(void)compare_two_panes(CT_CONTENTS, LT_ALL, 1, 0);

	assert_int_equal(4, lwin.list_rows);
	assert_int_equal(4, rwin.list_rows);

	lwin.list_pos = 0;

	assert_int_equal(0, fpos_prev_mismatch(&lwin));
	assert_int_equal(1, fpos_next_mismatch(&lwin));

	lwin.list_pos = 1;

	assert_int_equal(1, fpos_prev_mismatch(&lwin));
	assert_int_equal(2, fpos_next_mismatch(&lwin));

	lwin.list_pos = 2;

	assert_int_equal(1, fpos_prev_mismatch(&lwin));
	assert_int_equal(2, fpos_next_mismatch(&lwin));

	lwin.list_pos = 3;

	assert_int_equal(2, fpos_prev_mismatch(&lwin));
	assert_int_equal(3, fpos_next_mismatch(&lwin));

	view_teardown(&rwin);
}

TEST(current_unselected_file_is_marked)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH,
			"existing-files", cwd);
	load_dir_list(&lwin, 1);

	assert_int_equal(0, lwin.list_pos);
	assert_int_equal(3, lwin.list_rows);

	assert_int_equal(1, mark_selection_or_current(&lwin));

	assert_true(lwin.dir_entry[0].marked);
	assert_false(lwin.dir_entry[1].marked);
	assert_false(lwin.dir_entry[2].marked);
	assert_false(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);
	assert_false(lwin.dir_entry[2].selected);
}

TEST(selection_is_marked)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH,
			"existing-files", cwd);
	load_dir_list(&lwin, 1);

	assert_int_equal(0, lwin.list_pos);
	assert_int_equal(3, lwin.list_rows);

	lwin.selected_files = 1;
	lwin.dir_entry[1].selected = 1;
	assert_int_equal(1, mark_selection_or_current(&lwin));

	assert_false(lwin.dir_entry[0].marked);
	assert_true(lwin.dir_entry[1].marked);
	assert_false(lwin.dir_entry[2].marked);
	assert_false(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);
	assert_false(lwin.dir_entry[2].selected);
}

TEST(filelist_reloading_corrects_current_position)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH,
			"existing-files", cwd);
	load_dir_list(&lwin, 1);

	assert_int_equal(3, lwin.list_rows);
	lwin.list_pos = 2;
	assert_success(replace_matcher(&lwin.manual_filter, "/.*/"));
	load_dir_list(&lwin, 1);

	assert_int_equal(0, lwin.list_pos);
	assert_int_equal(1, lwin.list_rows);
}

TEST(fentry_get_size_returns_file_size_for_files)
{
	char origin[] = "/";
	const dir_entry_t entry = {
		.name = "f", .origin = origin, .size = 123U, .type = FT_REG
	};

	update_string(&cfg.shell, "");
	assert_success(stats_init(&cfg));

	assert_ulong_equal(entry.size, fentry_get_size(&lwin, &entry));

	update_string(&cfg.shell, NULL);
}

TEST(fentry_get_size_returns_file_size_if_nothing_cached)
{
	char origin[] = "/";
	const dir_entry_t entry = {
		.name = "f", .origin = origin, .size = 123U, .type = FT_DIR
	};

	update_string(&cfg.shell, "");
	assert_success(stats_init(&cfg));

	assert_ulong_equal(entry.size, fentry_get_size(&lwin, &entry));

	update_string(&cfg.shell, NULL);
}

TEST(fentry_get_recalculates_size)
{
	char dir_origin[] = SANDBOX_PATH;
	dir_entry_t dir = {
		.name = "dir", .origin = dir_origin, .size = 123U, .type = FT_DIR,
		.mtime = time(NULL)
	};
	char subdir_origin[] = SANDBOX_PATH "/dir";
	dir_entry_t subdir = {
		.name = "subdir", .origin = subdir_origin, .size = 123U, .type = FT_DIR,
		.mtime = time(NULL)
	};

	update_string(&cfg.shell, "");
	assert_success(stats_init(&cfg));

	assert_success(os_mkdir(SANDBOX_PATH "/dir", 0700));
	assert_success(os_mkdir(SANDBOX_PATH "/dir/subdir", 0700));

	(void)fops_dir_size(SANDBOX_PATH, 0, &no_cancellation);
	assert_ulong_equal(0, fentry_get_size(&lwin, &dir));
	assert_ulong_equal(0, fentry_get_size(&lwin, &subdir));

	copy_file(TEST_DATA_PATH "/various-sizes/block-size-file",
			SANDBOX_PATH "/dir/file");
	dir.mtime += 1000;
	assert_ulong_equal(8192, fentry_get_size(&lwin, &dir));
	assert_ulong_equal(0, fentry_get_size(&lwin, &subdir));

	copy_file(TEST_DATA_PATH "/various-sizes/block-size-file",
			SANDBOX_PATH "/dir/subdir/file");
	subdir.mtime += 1000;
	assert_ulong_equal(8192, fentry_get_size(&lwin, &subdir));
	assert_ulong_equal(16384, fentry_get_size(&lwin, &dir));

	assert_success(remove(SANDBOX_PATH "/dir/subdir/file"));
	subdir.mtime += 1000;
	assert_ulong_equal(0, fentry_get_size(&lwin, &subdir));
	assert_ulong_equal(8192, fentry_get_size(&lwin, &dir));

	assert_success(remove(SANDBOX_PATH "/dir/file"));
	dir.mtime += 1000;
	assert_ulong_equal(0, fentry_get_size(&lwin, &dir));
	assert_ulong_equal(0, fentry_get_size(&lwin, &subdir));

	assert_success(rmdir(SANDBOX_PATH "/dir/subdir"));
	assert_success(rmdir(SANDBOX_PATH "/dir"));

	update_string(&cfg.shell, NULL);
}

TEST(fentry_get_nitems_calculates_number_of_items)
{
	char origin[] = TEST_DATA_PATH;
	const dir_entry_t entry = {
		.name = "existing-files", .origin = origin, .type = FT_DIR
	};

	update_string(&cfg.shell, "");
	assert_success(stats_init(&cfg));

	assert_ulong_equal(3, fentry_get_nitems(&lwin, &entry));

	update_string(&cfg.shell, NULL);
}

TEST(fentry_get_nitems_recalculates_number_of_items)
{
	char origin[] = SANDBOX_PATH;
	dir_entry_t entry = {
		.name = ".", .origin = origin, .type = FT_DIR
	};

	update_string(&cfg.shell, "");
	assert_success(stats_init(&cfg));

	assert_ulong_equal(0, fentry_get_nitems(&lwin, &entry));

	create_file(SANDBOX_PATH "/a");
	entry.mtime = time(NULL) + 10;
	assert_ulong_equal(1, fentry_get_nitems(&lwin, &entry));

	update_string(&cfg.shell, NULL);

	assert_success(remove(SANDBOX_PATH "/a"));
}

TEST(root_path_does_not_get_more_than_one_slash, IF(not_windows))
{
	const char type_decs_slash[FT_COUNT][2][9] = {
		[FT_DIR][DECORATION_SUFFIX][0] = '/',
	};
	const char type_decs_brackets[FT_COUNT][2][9] = {
		[FT_DIR][DECORATION_PREFIX][0] = '[',
		[FT_DIR][DECORATION_SUFFIX][0] = ']',
	};

	char name[NAME_MAX + 1];
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "/");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	get_short_path_of(&lwin, &lwin.dir_entry[0], 1, 1, sizeof(name), name);
	assert_string_equal("/", name);

	memcpy(&cfg.type_decs, &type_decs_slash, sizeof(cfg.type_decs));

	get_short_path_of(&lwin, &lwin.dir_entry[0], 1, 1, sizeof(name), name);
	assert_string_equal("/", name);

	memcpy(&cfg.type_decs, &type_decs_brackets, sizeof(cfg.type_decs));
}

TEST(duplicated_entries_detected)
{
	lwin.list_rows = 2;
	lwin.list_pos = 0;
	lwin.top_line = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("lfile0");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].name = strdup("lfile0");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];

	check_file_uniqueness(&lwin);

	assert_int_equal(1, lwin.list_rows);
	assert_true(lwin.has_dups);
}

TEST(cache_handles_noexec_dirs, IF(regular_unix_user))
{
	assert_success(os_mkdir(SANDBOX_PATH "/dir", 0700));
	create_file(SANDBOX_PATH "/dir/file");
	assert_success(chmod(SANDBOX_PATH "/dir", 0666));

	cached_entries_t cache = {};
	assert_true(flist_update_cache(&lwin, &cache, SANDBOX_PATH "/dir"));
	assert_int_equal(0, cache.entries.nentries);
	flist_free_cache(&lwin, &cache);

	assert_success(chmod(SANDBOX_PATH "/dir", 0777));
	assert_success(remove(SANDBOX_PATH "/dir/file"));
	assert_success(rmdir(SANDBOX_PATH "/dir"));
}

TEST(filename_is_formatted_according_to_column_and_filetype)
{
	char origin[] = "";
	dir_entry_t file_entry = {
		.name = "a.b.c", .type = FT_REG, .origin = origin,
	};
	dir_entry_t dir_entry = {
		.name = "a.b.c", .type = FT_DIR, .origin = origin,
	};

	column_data_t cdt = { .view = &lwin };
	format_info_t info = { .data = &cdt };
	char name[16];

	memset(&cfg.type_decs, '\0', sizeof(cfg.type_decs));
	cfg.type_decs[FT_DIR][DECORATION_SUFFIX][0] = '/';

	cdt.entry = &dir_entry;

	info.id = SK_BY_INAME;
	format_name(NULL, sizeof(name), name, &info);
	assert_string_equal("a.b.c/", name);

	info.id = SK_BY_NAME;
	format_name(NULL, sizeof(name), name, &info);
	assert_string_equal("a.b.c/", name);

	info.id = SK_BY_FILEROOT;
	format_name(NULL, sizeof(name), name, &info);
	assert_string_equal("a.b.c/", name);

	info.id = SK_BY_ROOT;
	format_name(NULL, sizeof(name), name, &info);
	assert_string_equal("a.b/", name);

	cdt.entry = &file_entry;

	info.id = SK_BY_INAME;
	format_name(NULL, sizeof(name), name, &info);
	assert_string_equal("a.b.c", name);

	info.id = SK_BY_NAME;
	format_name(NULL, sizeof(name), name, &info);
	assert_string_equal("a.b.c", name);

	info.id = SK_BY_FILEROOT;
	format_name(NULL, sizeof(name), name, &info);
	assert_string_equal("a.b", name);

	info.id = SK_BY_ROOT;
	format_name(NULL, sizeof(name), name, &info);
	assert_string_equal("a.b", name);
}

TEST(fview_previews_works)
{
	lwin.list_rows = 2;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("file");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].name = strdup("dir");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].type = FT_DIR;
	strcpy(lwin.curr_dir, "/tests/fake");

	lwin.list_pos = 0;
	assert_false(fview_previews(&lwin, "/tests/fake/file"));
	lwin.list_pos = 1;
	assert_false(fview_previews(&lwin, "/tests/fake/dir"));
	assert_false(fview_previews(&lwin, "/unrelated/path"));

	lwin.miller_view = 1;
	lwin.list_pos = 0;
	assert_false(fview_previews(&lwin, "/tests/fake/file"));
	lwin.list_pos = 1;
	assert_false(fview_previews(&lwin, "/tests/fake/dir"));
	assert_false(fview_previews(&lwin, "/unrelated/path"));

	lwin.miller_preview_files = 1;
	lwin.list_pos = 0;
	assert_true(fview_previews(&lwin, "/tests/fake/file"));
	lwin.list_pos = 1;
	assert_false(fview_previews(&lwin, "/tests/fake/dir"));
	assert_false(fview_previews(&lwin, "/unrelated/path"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
