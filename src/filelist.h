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
#include "registers.h"

/* Type of contiguous area of file list. */
typedef enum
{
	FLS_SELECTION, /* Of selected entries. */
	FLS_MARKING,   /* Of marked entries. */
}
FileListScope;

/* Type of filter function for zapping list of entries.  Should return non-zero
 * if entry is to be kept and zero otherwise. */
typedef int (*zap_filter)(FileView *view, const dir_entry_t *entry, void *arg);

/* Initialization/termination functions. */

/* Prepares views for the first time. */
void init_filelists(void);
/* Reinitializes views. */
void reset_views(void);
/* Loads view file list for the first time. */
void load_initial_directory(FileView *view, const char dir[]);

/* Position related functions. */

/* Find index of the file within list of currently visible files of the view.
 * Returns file entry index or -1, if file wasn't found. */
int find_file_pos_in_list(const FileView *const view, const char file[]);
/* Find index of the file within list of currently visible files of the view.
 * Always matches file name and can optionally match directory if dir is not
 * NULL.  Returns file entry index or -1, if file wasn't found. */
int flist_find_entry(const FileView *view, const char file[], const char dir[]);
/* Tries to move cursor by pos_delta positions.  A wrapper for
 * correct_list_pos_on_scroll_up() and correct_list_pos_on_scroll_down()
 * functions. */
void correct_list_pos(FileView *view, ssize_t pos_delta);
/* Returns non-zero if doing something makes sense. */
int correct_list_pos_on_scroll_down(FileView *view, size_t lines_count);
/* Returns non-zero if doing something makes sense. */
int correct_list_pos_on_scroll_up(FileView *view, size_t lines_count);
/* Moves cursor to specified position. */
void flist_set_pos(FileView *view, int pos);
/* Ensures that position in the list doesn't exceed its bounds. */
void flist_ensure_pos_is_valid(FileView *view);
/* Ensures that cursor is moved outside of entries of certain type. */
void move_cursor_out_of(FileView *view, FileListScope scope);
/* Returns non-zero if cursor is on the first line. */
int at_first_line(const FileView *view);
/* Returns non-zero if cursor is on the last line. */
int at_last_line(const FileView *view);
/* Returns non-zero if cursor is on the first column. */
int at_first_column(const FileView *view);
/* Returns non-zero if cursor is on the last column. */
int at_last_column(const FileView *view);
/* Moves cursor to the first file in a row. */
void go_to_start_of_line(FileView *view);
/* Returns position of the first file in current line. */
int get_start_of_line(const FileView *view);
/* Returns position of the last file in current line. */
int get_end_of_line(const FileView *view);
/* Finds position of the next/previous group defined by primary sorting key.
 * Returns determined position (might point to the last/first entry in corner
 * cases). */
int flist_find_group(FileView *view, int next);
/* Finds position of the next/previous group defined by entries being files or
 * directories.  Returns determined position (might point to the last/first
 * entry in corner cases). */
int flist_find_dir_group(FileView *view, int next);

/* Appearance related functions. */

/* Inverts primary key sorting order. */
void invert_sorting_order(FileView *view);
/* Returns non-zero in case view is visible and shows list of files at the
 * moment. */
int window_shows_dirlist(const FileView *const view);
void change_sort_type(FileView *view, char type, char descending);

/* Directory traversing functions. */

/* Changes current directory of the view to the path if it's possible and in
 * case of success reloads filelist of the view and sets its cursor position
 * according to directory history of the view. */
void navigate_to(FileView *view, const char path[]);
/* Changes current directory of the view to location the view was before last
 * directory change. */
void navigate_back(FileView *view);
/* Changes current directory of the view to the dir if it's possible and in case
 * of success reloads filelist of the view and sets its cursor position on the
 * file trying to ensure that it's visible.  If preserve_cv is set, tries to do
 * not reset custom view when possible. */
void navigate_to_file(FileView *view, const char dir[], const char file[],
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
int change_directory(FileView *view, const char path[]);
/* Changes pane directory handling path just like cd command does (e.g. "-" goes
 * to previous location and NULL or empty path goes to home directory). */
int cd(FileView *view, const char base_dir[], const char path[]);
/* Ensures that current directory of the view is a valid one.  Modifies
 * view->curr_dir. */
void leave_invalid_dir(FileView *view);
/* Checks if the view is the directory specified by the path.  Returns non-zero
 * if so, otherwise zero is returned. */
int pane_in_dir(const FileView *view, const char path[]);

/* Selection related functions. */

/* Cleans selection possibly saving it for later use. */
void clean_selected_files(FileView *view);
/* Erases selection not saving anything. */
void erase_selection(FileView *view);
/* Inverts selection of files in the view. */
void invert_selection(FileView *view);
/* Reselects previously selected entries.  When reg is NULL, saved selection is
 * restored, otherwise list of files to restore is taken from the register. */
void flist_sel_restore(FileView *view, reg_t *reg);
/* Counts number of selected files and writes saves the number in
 * view->selected_files. */
void recount_selected_files(FileView *view);
/* Remove dot and regexp filters if it's needed to make file visible.  Returns
 * non-zero if file was found. */
int ensure_file_is_selected(FileView *view, const char name[]);

/* Directory history related functions. */

/* Changes current directory of the view to next location backward in
 * history, if available. */
void navigate_backward_in_history(FileView *view);
/* Changes current directory of the view to next location forward in history, if
 * available. */
void navigate_forward_in_history(FileView *view);
/* Adds new entry to directory history of the view or updates an existing entry.
 * If path is NULL, current path is used.  If file is NULL current file is used.
 * If pos is negative, current position is used.  Empty file name signifies
 * visiting directory, which shouldn't reset name of previously active file in
 * it. */
void save_view_history(FileView *view, const char path[], const char file[],
		int pos);
int is_in_view_history(FileView *view, const char *path);
void clean_positions_in_history(FileView *view);
/* Looks up history in the source to update cursor position in the view. */
void flist_hist_lookup(FileView *view, const FileView *source);

/* Typed (with trailing slash for directories) file name function. */

/* Gets typed path for the entry.  On return allocates memory, that should be
 * freed by the caller. */
char * get_typed_entry_fpath(const dir_entry_t *entry);

/* Custom file list functions. */

/* Checks whether view displays custom list of files.  Returns non-zero if so,
 * otherwise zero is returned. */
int flist_custom_active(const FileView *view);
/* Prepares list of files for it to be filled with entries. */
void flist_custom_start(FileView *view, const char title[]);
/* Adds an entry to list of files. */
void flist_custom_add(FileView *view, const char path[]);
/* Finishes file list population, handles empty resulting list corner case.
 * Returns zero on success, otherwise (on empty list) non-zero is returned. */
int flist_custom_finish(FileView *view, int very);
/* Removes selected files from custom view. */
void flist_custom_exclude(FileView *view);
/* Clones list of files from from view to to view. */
void flist_custom_clone(FileView *to, const FileView *from);

/* Other functions. */

/* Gets path to current directory of the view.  Returns the path. */
const char * flist_get_dir(const FileView *view);
/* Selects entry that corresponds to the path as the current one. */
void flist_goto_by_path(FileView *view, const char path[]);
/* Loads filelist for the view, but doesn't redraw the view.  The reload
 * parameter should be set in case of view refresh operation. */
void populate_dir_list(FileView *view, int reload);
/* Loads file list for the view and redraws the view.  The reload parameter
 * should be set in case of view refresh operation. */
void load_dir_list(FileView *view, int reload);
/* Resorts view without reloading it and preserving current file under cursor
 * along with its relative position in the list.  msg parameter controls whether
 * to show "Sorting..." statusbar message. */
void resort_dir_list(int msg, FileView *view);
void load_saving_pos(FileView *view, int reload);
char * get_current_file_name(FileView *view);
/* Gets current entry of the view.  Returns the entry or NULL if view doesn't
 * contain any. */
dir_entry_t * get_current_entry(FileView *view);
/* Checks whether content in the current directory of the view changed and
 * reloads the view if so. */
void check_if_filelist_have_changed(FileView *view);
/* Checks whether cd'ing into path is possible. Shows cd errors to a user.
 * Returns non-zero if it's possible, zero otherwise. */
int cd_is_possible(const char *path);
/* Checks whether directory list was loaded at least once since startup. */
int is_dir_list_loaded(FileView *view);
/* Checks whether view can and should be navigated to the path (no need to do
 * anything if already there).  Returns non-zero if path should be changed. */
int view_needs_cd(const FileView *view, const char path[]);
/* Sets view's current directory from path value. */
void set_view_path(FileView *view, const char path[]);
/* Returns possible cached or calculated value of file size. */
uint64_t get_file_size_by_entry(const FileView *view, size_t pos);
/* Checks whether entry corresponds to a directory.  Returns non-zero if so,
 * otherwise zero is returned. */
int is_directory_entry(const dir_entry_t *entry);
/* Loads pointer to the next selected entry in file list of the view.  *entry
 * should be NULL for the first call and result of previous call otherwise.
 * Returns zero when there is no more entries to supply, otherwise non-zero is
 * returned.  List of entries shouldn't be reloaded between invocations of this
 * function. */
int iter_selected_entries(FileView *view, dir_entry_t **entry);
/* Same as iter_selected_entries() function, but traverses selected items only
 * if current element is selected, otherwise only current element is
 * processed. */
int iter_active_area(FileView *view, dir_entry_t **entry);
/* Same as iter_selected_entries() function, but checks for marks. */
int iter_marked_entries(FileView *view, dir_entry_t **entry);
/* Same as iter_selected_entries() function, but when selection is absent
 * current file is processed. */
int iter_selection_or_current(FileView *view, dir_entry_t **entry);
/* Maps one of file list entries to its position in the list.  Returns the
 * position or -1 on wrong entry. */
int entry_to_pos(const FileView *view, const dir_entry_t *entry);
/* Fills the buffer with the full path to file under cursor. */
void get_current_full_path(const FileView *view, size_t buf_len, char buf[]);
/* Fills the buffer with the full path to file at specified position. */
void get_full_path_at(const FileView *view, int pos, size_t buf_len,
		char buf[]);
/* Fills the buffer with the full path to file of specified file list entry. */
void get_full_path_of(const dir_entry_t *entry, size_t buf_len, char buf[]);
/* Fills the buffer with short path of specified file list entry.  The
 * shortening occurs for files under original directory of custom views.
 * Non-zero format enables file type specific decoration. */
void get_short_path_of(const FileView *view, const dir_entry_t *entry,
		int format, size_t buf_len, char buf[]);
/* Ensures that either entries at specified positions, selected entries or file
 * under cursor is marked. */
void check_marking(FileView *view, int count, const int indexes[]);
/* Marks files at positions specified in the indexes array of size count. */
void mark_files_at(FileView *view, int count, const int indexes[]);
/* Marks selected files of the view. */
void mark_selected(FileView *view);
/* Same as mark_selected() function, but when selection is absent current file
 * is marked. */
void mark_selection_or_current(FileView *view);
/* Counts number of marked files that can be processed (thus excluding parent
 * directory, which is "..").  Returns the count. */
int flist_count_marked(FileView *const view);
/* Removes dead entries (those that refer to non-existing files) or those that
 * do not match local filter from the view.  Returns number of erased
 * entries. */
int zap_entries(FileView *view, dir_entry_t *entries, int *count,
		zap_filter filter, void *arg, int allow_empty_list);
/* Finds directory entry in the list of entries by the path.  Returns pointer to
 * the found entry or NULL. */
dir_entry_t * entry_from_path(dir_entry_t *entries, int count,
		const char path[]);
/* Retrieves number of items in a directory specified by the entry.  Returns the
 * number, which is zero for files. */
uint64_t entry_get_nitems(FileView *view, const dir_entry_t *entry);
/* Calculates number of items at path specified by the entry.  No check for file
 * type is performed.  Returns the number, which is zero for files. */
uint64_t entry_calc_nitems(const dir_entry_t *entry);
/* Replaces all entries of the *entries with copy of with_entries elements. */
void replace_dir_entries(FileView *view, dir_entry_t **entries, int *count,
		const dir_entry_t *with_entries, int with_count);
/* Adds new entry to the *list of length *list_size and updates them
 * appropriately.  Returns zero on success, otherwise non-zero is returned. */
int add_dir_entry(dir_entry_t **list, size_t *list_size,
		const dir_entry_t *entry);
/* Frees single directory entry. */
void free_dir_entry(const FileView *view, dir_entry_t *entry);
/* Adds parent directory entry (..) to filelist. */
void add_parent_dir(FileView *view);
/* Loads list of paths (absolute or relative to the path) into custom view.
 * Exists with error message on failed attempt. */
void flist_set(FileView *view, const char title[], const char path[],
		char *lines[], int nlines);
/* Parses line to extract path and adds it to custom view or does nothing. */
void flist_add_custom_line(FileView *view, const char line[]);
/* A more high level version of flist_custom_finish(), which takes care of error
 * handling and cursor position. */
void flist_end_custom(FileView *view, int very);
/* Changes name of a file entry, performing additional required updates. */
void fentry_rename(dir_entry_t *entry, const char to[]);

TSTATIC_DEFS(
	TSTATIC void pick_cd_path(FileView *view, const char base_dir[],
			const char path[], int *updir, char buf[], size_t buf_size);
)

#endif /* VIFM__FILELIST_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
