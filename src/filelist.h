/* vifm
 * Copyright (C) 2001 Ken Steen.
 * Copyright (C) 2011 xaizek.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef VIFM__FILELIST_H__
#define VIFM__FILELIST_H__

#include <sys/types.h> /* ssize_t */

#include <stddef.h> /* size_t */
#include <stdint.h> /* uint64_t */

#include "ui/ui.h"
#include "utils/test_helpers.h"

/* Type of filter function for zapping list of entries.  Should return non-zero
 * if entry is to be kept and zero otherwise. */
typedef int (*zap_filter)(view_t *view, const dir_entry_t *entry, void *arg);

/* Type of predicate functions to reason about entries.  Should return non-zero
 * if particular property holds and zero otherwise. */
typedef int (*entry_predicate)(const dir_entry_t *entry);

/* Initialization/termination functions. */

/* Prepares views for the first time. */
void init_filelists(void);
/* Loads initial display values into view structure. */
void flist_init_view(view_t *view);
/* Frees all resources allocated by the view and prepares it for future
 * reuse. */
void flist_free_view(view_t *view);
/* Reinitializes views. */
void reset_views(void);
/* Loads view file list for the first time. */
void load_initial_directory(view_t *view, const char dir[]);

/* Appearance related functions. */

/* Inverts primary key sorting order. */
void invert_sorting_order(view_t *view);
/* Returns non-zero in case view is visible and shows list of files at the
 * moment. */
int window_shows_dirlist(const view_t *view);
void change_sort_type(view_t *view, char type, char descending);

/* Directory traversing functions. */

/* Changes current directory of the view to the path if it's possible and in
 * case of success reloads filelist of the view and sets its cursor position
 * according to directory history of the view.  Returns zero on success,
 * otherwise non-zero is returned. */
int navigate_to(view_t *view, const char path[]);
/* Changes current directory of the view to location the view was before last
 * directory change. */
void navigate_back(view_t *view);
/* Changes current directory of the view to the dir if it's possible and in case
 * of success reloads filelist of the view and sets its cursor position on the
 * file trying to ensure that it's visible.  If preserve_cv is set, tries to do
 * not reset custom view when possible. */
void navigate_to_file(view_t *view, const char dir[], const char file[],
		int preserve_cv);
/* The directory can either be relative to the current
 * directory - ../
 * or an absolute path - /usr/local/share
 * The *directory passed to change_directory() cannot be modified.
 * Symlink directories require an absolute path
 *
 * Return value:
 *  -1  if there were errors.
 *   0  if directory successfully changed and we didn't leave FUSE mount
 *      directory.
 *   1  if directory successfully changed and we left FUSE mount directory. */
int change_directory(view_t *view, const char path[]);
/* Changes pane directory handling path just like cd command does (e.g. "-" goes
 * to previous location and NULL or empty path goes to home directory). */
int cd(view_t *view, const char base_dir[], const char path[]);
/* Ensures that current directory of the view is a valid one.  Modifies
 * view->curr_dir. */
void leave_invalid_dir(view_t *view);
/* Checks if the view is the directory specified by the path.  Returns non-zero
 * if so, otherwise zero is returned. */
int pane_in_dir(const view_t *view, const char path[]);
/* Navigates to next/previous sibling directory (does nothing for root or custom
 * view) with optional wrapping.  Returns non-zero if status bar message should
 * be preserved. */
int go_to_sibling_dir(view_t *view, int offset, int wrap);
/* Picks new directory or requested going up one level judging from supplied
 * base directory, desired location and current location of the view.  Buffer is
 * not filled if *updir gets set. */
void flist_pick_cd_path(view_t *view, const char base_dir[], const char path[],
		int *updir, char buf[], size_t buf_size);

/* Typed (with trailing slash for directories) file name function. */

/* Gets typed path for the entry.  On return allocates memory, that should be
 * freed by the caller. */
char * get_typed_entry_fpath(const dir_entry_t *entry);

/* Custom file list functions. */

/* Checks whether view displays custom list of files.  Returns non-zero if so,
 * otherwise zero is returned. */
int flist_custom_active(const view_t *view);
/* Prepares list of files for it to be filled with entries. */
void flist_custom_start(view_t *view, const char title[]);
/* Adds an entry to custom list of files.  Returns pointer to just added entry
 * or NULL on error. */
dir_entry_t * flist_custom_add(view_t *view, const char path[]);
/* Puts an entry to custom list of files, contents of the entry gets stolen.
 * Returns pointer to just added entry or NULL on error. */
dir_entry_t * flist_custom_put(view_t *view, dir_entry_t *entry);
/* Parses line to extract path and adds it to custom view or does nothing. */
void flist_custom_add_spec(view_t *view, const char line[]);
/* Appends entry separator to the list with specified id. */
void flist_custom_add_separator(view_t *view, int id);
/* Finishes file list population, handles empty resulting list corner case.
 * Non-zero allow_empty makes a single-entry (..) view instead of aborting.
 * Returns zero on success, otherwise (on empty list) non-zero is returned. */
int flist_custom_finish(view_t *view, CVType type, int allow_empty);
/* A more high level version of flist_custom_finish(), which takes care of error
 * handling and cursor position. */
void flist_custom_end(view_t *view, int very);
/* Loads list of paths (absolute or relative to the path) into custom view.
 * Exists with error message on failed attempt. */
void flist_custom_set(view_t *view, const char title[], const char path[],
		char *lines[], int nlines);
/* Removes active files from custom view.  Zero selection_only enables excluding
 * files that share ids with selected items. */
void flist_custom_exclude(view_t *view, int selection_only);
/* Clones list of files from from view to to view. */
void flist_custom_clone(view_t *to, const view_t *from, int as_tree);
/* Adds missing parent directories to the tree.  Such entries are marked as
 * "temporary", which can be used to remove them later. */
void flist_custom_uncompress_tree(view_t *view);
/* Save custom list for further use so it can be reloaded later in full.  Should
 * be invoked before removing entries.  Does nothing for FS-backed views or if
 * list was already saved. */
void flist_custom_save(view_t *view);

/* Other functions. */

/* Gets path to current directory of the view.  Returns the path. */
const char * flist_get_dir(const view_t *view);
/* Selects entry that corresponds to the path as the current one. */
void flist_goto_by_path(view_t *view, const char path[]);
/* Loads filelist for the view, but doesn't redraw the view.  The reload
 * parameter should be set in case of view refresh operation mainly to avoid
 * synchronizing cursor position with contents of history.  Returns non-zero on
 * error, otherwise zero is returned. */
int populate_dir_list(view_t *view, int reload);
/* Loads file list for the view and redraws the view.  The reload parameter
 * should be set in case of view refresh operation. */
void load_dir_list(view_t *view, int reload);
/* Resorts view without reloading it and preserving current file under cursor
 * along with its relative position in the list.  msg parameter controls whether
 * to show "Sorting..." status bar message. */
void resort_dir_list(int msg, view_t *view);
/* Reloads file list while preserving cursor position if possible. */
void load_saving_pos(view_t *view);
char * get_current_file_name(view_t *view);
/* Gets current entry of the view.  Returns the entry or NULL if view doesn't
 * contain any. */
dir_entry_t * get_current_entry(const view_t *view);
/* Checks whether content in the current directory of the view changed and
 * reloads the view if so. */
void check_if_filelist_has_changed(view_t *view);
/* Checks whether cd'ing into path is possible. Shows cd errors to a user.
 * Returns non-zero if it's possible, zero otherwise. */
int cd_is_possible(const char path[]);
/* Checks whether directory list was loaded at least once since startup. */
int is_dir_list_loaded(view_t *view);
/* Checks whether view can and should be navigated to the path (no need to do
 * anything if already there).  Returns non-zero if path should be changed. */
int view_needs_cd(const view_t *view, const char path[]);
/* Sets view's current directory from path value. */
void set_view_path(view_t *view, const char path[]);
/* Retrieves size of the entry, possibly using cached or calculated value.
 * Returns the size. */
uint64_t fentry_get_size(const view_t *view, const dir_entry_t *entry);
/* Loads pointer to the next selected entry in file list of the view.  *entry
 * should be NULL for the first call and result of previous call otherwise.
 * Returns zero when there is no more entries to supply, otherwise non-zero is
 * returned.  List of entries shouldn't be reloaded between invocations of this
 * function. */
int iter_selected_entries(view_t *view, dir_entry_t **entry);
/* Same as iter_selected_entries() function, but checks for marks. */
int iter_marked_entries(view_t *view, dir_entry_t **entry);
/* Same as iter_selected_entries() function, but when selection is absent
 * current file is processed. */
int iter_selection_or_current(view_t *view, dir_entry_t **entry);
/* Maps one of file list entries to its position in the list.  Returns the
 * position or -1 on wrong entry. */
int entry_to_pos(const view_t *view, const dir_entry_t *entry);
/* Fills the buffer with the full path to file under cursor.  Set to empty if
 * there are no files in the view. */
void get_current_full_path(const view_t *view, size_t buf_len, char buf[]);
/* Fills the buffer with the full path to file at specified position.  Set to
 * empty if there are no files in the view. */
void get_full_path_at(const view_t *view, int pos, size_t buf_len, char buf[]);
/* Fills the buffer with the full path to file of specified file list entry. */
void get_full_path_of(const dir_entry_t *entry, size_t buf_len, char buf[]);
/* Fills the buffer with short path of specified file list entry.  The
 * shortening occurs for files under original directory of custom views.
 * Non-zero drop_prefix requests omitting file prefix for trees. */
void get_short_path_of(const view_t *view, const dir_entry_t *entry,
		NameFormat fmt, int drop_prefix, size_t buf_len, char buf[]);
/* Ensures that either entries at specified positions, pending marking, selected
 * entries or file under cursor is marked. */
void check_marking(view_t *view, int count, const int indexes[]);
/* Ensures that either pending marking, selected entries or file under cursor is
 * marked.  Non-zero prefer_current parameter makes marking selection contingent
 * on current file being selected. */
void flist_set_marking(view_t *view, int prefer_current);
/* Unmarks all entries of the view. */
void clear_marking(view_t *view);
/* Marks files at positions specified in the indexes array of size count. */
void mark_files_at(view_t *view, int count, const int indexes[]);
/* Marks selected files of the view,  but when selection is absent current file
 * is marked.  Returns number of marked files. */
int mark_selection_or_current(view_t *view);
/* Counts marked entries of a view.  Returns the count. */
int flist_count_marked(const view_t *view);
/* Removes dead entries (those that refer to non-existing files) or those that
 * do not match local filter from the view.  remove_subtrees flag controls
 * whether child nodes should be removed as well.  Returns number of erased
 * entries. */
int zap_entries(view_t *view, dir_entry_t *entries, int *count,
		zap_filter filter, void *arg, int allow_empty_list, int remove_subtrees);
/* Leaves only those entries in compare view, for which filter returns non-zero.
 * Properly updates the other pane.  Returns non-zero if views were left,
 * because they became empty. */
int filter_in_compare(view_t *view, void *arg, zap_filter filter);
/* Finds directory entry in the list of entries by the path.  Returns pointer to
 * the found entry or NULL. */
dir_entry_t * entry_from_path(view_t *view, dir_entry_t *entries, int count,
		const char path[]);
/* Retrieves number of items in a directory specified by the entry.  Returns the
 * number, which is zero for files. */
uint64_t fentry_get_nitems(const view_t *view, const dir_entry_t *entry);
/* Queries information about a directory from dcache.  *size might be set to
 * DCACHE_UNKNOWN. */
void fentry_get_dir_info(const view_t *view, const dir_entry_t *entry,
		uint64_t *size, uint64_t *nitems);
/* Checks whether entry is selected.  Returns non-zero if so, otherwise zero is
 * returned. */
int is_entry_selected(const dir_entry_t *entry);
/* Checks whether entry is marked.  Returns non-zero if so, otherwise zero is
 * returned. */
int is_entry_marked(const dir_entry_t *entry);
/* Replaces all entries of the *entries with copy of with_entries elements. */
void replace_dir_entries(view_t *view, dir_entry_t **entries, int *count,
		const dir_entry_t *with_entries, int with_count);
/* Adds new entry to the *list of length *list_size and updates them
 * appropriately.  Returns NULL on error, otherwise pointer to the entry is
 * returned. */
dir_entry_t * add_dir_entry(dir_entry_t **list, size_t *list_size,
		const dir_entry_t *entry);
/* Adds new entry to the list and fills it with data.  Returns pointer to the
 * entry or NULL on error. */
dir_entry_t * entry_list_add(view_t *view, dir_entry_t **list, int *list_size,
		const char path[]);
/* Frees list of directory entries related to the view.  Sets *entries and
 * *count to safe values. */
void free_dir_entries(view_t *view, dir_entry_t **entries, int *count);
/* Frees single directory entry. */
void fentry_free(const view_t *view, dir_entry_t *entry);
/* Adds parent directory entry (..) to filelist. */
void add_parent_dir(view_t *view);
/* Changes name of a file entry, performing additional required updates. */
void fentry_rename(view_t *view, dir_entry_t *entry, const char to[]);
/* Checks whether this is fake entry for internal purposes, which should not be
 * processed as a file. */
int fentry_is_fake(const dir_entry_t *entry);
/* Checks whether this is valid entry, which can be selected and processed by
 * file operations or even just selected.  Currently this checks for entry not
 * being ".." nor fake. */
int fentry_is_valid(const dir_entry_t *entry);
/* Checks whether entry corresponds to a directory (including symbolic links to
 * directories).  Returns non-zero if so, otherwise zero is returned. */
int fentry_is_dir(const dir_entry_t *entry);
/* Checks whether entry points to a path resolving symbolic links if necessary.
 * Returns non-zero if so, otherwise zero is returned. */
int fentry_points_to(const dir_entry_t *entry, const char path[]);
/* Loads directory tree specified by its path into the view.  The depth
 * parameter can be used to limit nesting level (>= 0).  Considers various
 * filters.  Returns zero on success, otherwise non-zero is returned. */
int flist_load_tree(view_t *view, const char path[], int depth);
/* Makes to contain tree with the same root as from including copying list of
 * excluded files.  Returns zero on success, otherwise non-zero is returned. */
int flist_clone_tree(view_t *to, const view_t *from);
/* Updates specified cache of the view.  If the path is NULL, then nothing is
 * done.  Returns non-zero if cached file list has changed, otherwise zero is
 * returned. */
int flist_update_cache(view_t *view, cached_entries_t *cache,
		const char path[]);
/* Frees the cache. */
void flist_free_cache(view_t *view, cached_entries_t *cache);
/* Updates non-heap-allocated origin pointers of entries in file list
 * entries. */
void flist_update_origins(view_t *view);
/* Toggles fold of the current entry if applicable. */
void flist_toggle_fold(view_t *view);
/* Checks whether file list synchronizes with FS.  Returns non-zero if so,
 * otherwise zero is returned. */
int flist_is_fs_backed(const view_t *view);

TSTATIC_DEFS(
	void check_file_uniqueness(view_t *view);
)

#endif /* VIFM__FILELIST_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
