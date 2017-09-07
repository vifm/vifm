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

#ifndef VIFM__FLIST_POS_H__
#define VIFM__FLIST_POS_H__

/* This unit provides functions related to positioning/scrolling in file
 * lists. */

struct view_t;

/* Type of contiguous area within list of files. */
typedef enum
{
	FLS_SELECTION, /* Of selected entries. */
	FLS_MARKING,   /* Of marked entries. */
}
FileListScope;

/* Finds index of the file within list of currently visible files of the view.
 * Returns file entry index or -1 if file wasn't found. */
int find_file_pos_in_list(const struct view_t *const view, const char file[]);

/* Finds index of the file within list of currently visible files of the view.
 * Always matches file name and can optionally match directory if dir is not
 * NULL.  Returns file entry index or -1 if file wasn't found. */
int flist_find_entry(const struct view_t *view, const char file[],
		const char dir[]);

/* Tries to move cursor down by given number of lines.  Returns non-zero if
 * something was updated. */
int correct_list_pos_on_scroll_down(struct view_t *view, int lines_count);

/* Tries to move cursor up by given number of lines.  Returns non-zero if
 * something was updated. */
int correct_list_pos_on_scroll_up(struct view_t *view, int lines_count);

/* Moves cursor to specified position. */
void flist_set_pos(struct view_t *view, int pos);

/* Ensures that position in the list doesn't exceed its bounds. */
void flist_ensure_pos_is_valid(struct view_t *view);

/* Ensures that cursor is moved outside of entries of certain type. */
void move_cursor_out_of(struct view_t *view, FileListScope scope);

/* Retrieves column number (base zero) of the specified position (cell number
 * base zero).  Returns the number. */
int fpos_get_col(const struct view_t *view, int pos);

/* Retrieves line number (base zero) of the specified position (cell number base
 * zero).  Returns the number. */
int fpos_get_line(const struct view_t *view, int pos);

/* Checks whether it's possible to move cursor from current position to the
 * left.  Returns non-zero if so, otherwise zero is returned. */
int fpos_can_move_left(const struct view_t *view);

/* Checks whether it's possible to move cursor from current position to the
 * right.  Returns non-zero if so, otherwise zero is returned. */
int fpos_can_move_right(const struct view_t *view);

/* Checks whether it's possible to move cursor from current position up.
 * Returns non-zero if so, otherwise zero is returned. */
int fpos_can_move_up(const struct view_t *view);

/* Checks whether it's possible to move cursor from current position down.
 * Returns non-zero if so, otherwise zero is returned. */
int fpos_can_move_down(const struct view_t *view);

/* Checks whether cursor is on the first column.  Returns non-zero if so,
 * otherwise zero is returned. */
int at_first_column(const struct view_t *view);

/* Checks whether cursor is on the last column.  Returns non-zero if so,
 * otherwise zero is returned. */
int at_last_column(const struct view_t *view);

/* Calculates position of the first file in current line.  Returns the
 * position. */
int get_start_of_line(const struct view_t *view);

/* Calculates position of the last file in current line.  Returns the
 * position. */
int get_end_of_line(const struct view_t *view);

/* Retrieves step in files that's used to move within a line.  Retrieves the
 * step. */
int fpos_get_hor_step(const struct view_t *view);

/* Retrieves step in files that's used to move within a column.  Retrieves the
 * step. */
int fpos_get_ver_step(const struct view_t *view);

/* Checks whether there are more elements to show above what can be seen
 * currently.  Returns non-zero if so, otherwise zero is returned. */
int fpos_has_hidden_top(const struct view_t *view);

/* Checks whether there are more elements to show below what can be seen
 * currently.  Returns non-zero if so, otherwise zero is returned. */
int fpos_has_hidden_bottom(const struct view_t *view);

/* Calculates position in list of files that corresponds to window top, which is
 * adjusted according to 'scrolloff' option.  Returns the position. */
int fpos_get_top_pos(const struct view_t *view);

/* Calculates position in list of files that corresponds to window middle, which
 * is adjusted according to 'scrolloff' option.  Returns the position. */
int fpos_get_middle_pos(const struct view_t *view);

/* Calculates position in list of files that corresponds to window bottom, which
 * is adjusted according to 'scrolloff' option.  Returns the position. */
int fpos_get_bottom_pos(const struct view_t *view);

/* Retrieves scroll offset value for the view taking view height into account.
 * Returns the effective scroll offset. */
int fpos_get_offset(const struct view_t *view);

/* Checks whether if all files are visible, so no scrolling is needed.  Returns
 * non-zero if so, and zero otherwise. */
int fpos_are_all_files_visible(const struct view_t *view);

/* Gets file position of last visible cell in the view.  Value returned may be
 * greater than or equal to the number of files in the view and thus should be
 * treated correctly.  Returns the index. */
int fpos_get_last_visible_cell(const struct view_t *view);

/* Scrolls view by a half-page.  Updates top line, but not list position to make
 * it possible to process files in the range from old to new position.  Returns
 * new list position. */
int fpos_half_scroll(struct view_t *view, int down);

/* Finds position of the next/previous group defined by primary sorting key.
 * Returns determined position (might point to the last/first entry in corner
 * cases). */
int flist_find_group(const struct view_t *view, int next);

/* Finds position of the next/previous group defined by entries being files or
 * directories.  Returns determined position (might point to the last/first
 * entry in corner cases). */
int flist_find_dir_group(const struct view_t *view, int next);

/* Finds position of the first child of the parent of the current node.  Returns
 * new position which isn't changed if already at first child. */
int flist_first_sibling(const struct view_t *view);

/* Finds position of the last child of the parent of the current node.  Returns
 * new position which isn't changed if already at last child. */
int flist_last_sibling(const struct view_t *view);

/* Finds position of the next sibling directory entry.  Returns new position
 * which isn't changed if nothing was found. */
int flist_next_dir_sibling(const struct view_t *view);

/* Finds position of the previous sibling directory entry.  Returns new position
 * which isn't changed if nothing was found. */
int flist_prev_dir_sibling(const struct view_t *view);

/* Finds position of the next directory entry.  Returns new position which isn't
 * changed if no next directory is found. */
int flist_next_dir(const struct view_t *view);

/* Finds position of the previous directory entry.  Returns new position which
 * isn't changed if no previous directory is found. */
int flist_prev_dir(const struct view_t *view);

/* Finds position of the next selected entry.  Returns new position which isn't
 * changed if no next selected entry is found. */
int flist_next_selected(const struct view_t *view);

/* Finds position of the previous selected entry.  Returns new position which
 * isn't changed if no previous selected entry is found. */
int flist_prev_selected(const struct view_t *view);

/* Finds position of the next mismatched entry.  Returns new position which
 * isn't changed if no next such entry is found. */
int flist_next_mismatch(const struct view_t *view);

/* Finds position of the previous mismatched entry.  Returns new position which
 * isn't changed if no previous such entry is found. */
int flist_prev_mismatch(const struct view_t *view);

/* Remove dot and regexp filters if it's needed to make file visible.  Returns
 * non-zero if file was found. */
int ensure_file_is_selected(struct view_t *view, const char name[]);

/* Finds next/previous file which starts with the given character.  Returns
 * -1 if nothing was found, otherwise new position.  When wrapping, can also
 * return current position to signify that there is nowhere to move (no check
 * whether it matches is performed). */
int flist_find_by_ch(const struct view_t *view, int ch, int backward, int wrap);

#endif /* VIFM__FLIST_POS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
