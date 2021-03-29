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

#ifndef VIFM__UI__FILEVIEW_H__
#define VIFM__UI__FILEVIEW_H__

#include <stddef.h> /* size_t */

#include "../utils/test_helpers.h"

struct dir_entry_t;
struct view_t;

/* Packet set of parameters to pass as user data for processing columns. */
typedef struct
{
	struct view_t *view;       /* View on which cell is being drawn. */
	struct dir_entry_t *entry; /* Entry that is being displayed. */

	int line_pos;      /* File position in the file list (the view).  Can be -1
	                    * for filler (entry should still be supplied though). */
	int line_hi_group; /* Line highlight (to avoid per-column calculation). */
	int current_pos;   /* Position of entry selected with the cursor. */
	int total_width;   /* Total width available for drawing. */
	int number_width;  /* Whether to draw line numbers. */

	size_t current_line;  /* Line of the cell within the view window. */
	size_t column_offset; /* Offset in characters of the column. */

	size_t *prefix_len; /* Data prefix length (should be drawn in neutral color).
	                     * A pointer to allow changing value in const struct.
	                     * Should be zero first time, then auto reset. */
	int is_main;        /* Whether this is main file list. */

	int custom_match;   /* Whether the keys below have meaningful values. */
	int match_from;     /* Start offset of the match. */
	int match_to;       /* End offset of the match. */
}
column_data_t;

/* Initialization/termination functions. */

/* Initializes file view unit. */
void fview_setup(void);

/* Initializes view once before. */
void fview_init(struct view_t *view);

/* Resets view state partially. */
void fview_reset(struct view_t *view);

/* Resets view state with regard to color schemes. */
void fview_reset_cs(struct view_t *view);

/* Appearance related functions. */

/* Redraws directory list and puts inactive mark for the other view. */
void draw_dir_list(struct view_t *view);

/* Redraws directory list without putting inactive mark if view == other. */
void draw_dir_list_only(struct view_t *view);

/* Updates view (maybe postponed) on the screen (redraws file list and
 * cursor). */
void redraw_view(struct view_t *view);

/* Updates current view (maybe postponed) on the screen (redraws file list and
 * cursor) */
void redraw_current_view(void);

/* Draws inactive cursor on the view. */
void fview_draw_inactive_cursor(struct view_t *view);

/* Redraws cursor of the view on the screen. */
void fview_cursor_redraw(struct view_t *view);

/* Scrolling related functions. */

/* Checks if view can be scrolled up (there are more files).  Returns non-zero
 * if so, and zero otherwise. */
int can_scroll_up(const struct view_t *view);

/* Checks if view can be scrolled down (there are more files).  Returns non-zero
 * if so, and zero otherwise. */
int can_scroll_down(const struct view_t *view);

/* Scrolls view up at least by specified number of files.  Updates both top and
 * cursor positions. */
void scroll_up(struct view_t *view, int by);

/* Scrolls view down at least by specified number of files.  Updates both top
 * and cursor positions. */
void scroll_down(struct view_t *view, int by);

/* Calculates list position corrected for scrolling down.  Returns adjusted
 * position. */
int get_corrected_list_pos_down(const struct view_t *view, int pos_delta);

/* Calculates list position corrected for scrolling up.  Returns adjusted
 * position. */
int get_corrected_list_pos_up(const struct view_t *view, int pos_delta);

/* Updates current and top line of a view according to 'scrolloff' option value.
 * Returns non-zero if redraw is needed. */
int consider_scroll_offset(struct view_t *view);

/* Scrolls view down or up at least by specified number of files.  Updates both
 * top and cursor positions.  A wrapper for scroll_up() and scroll_down()
 * functions. */
void scroll_by_files(struct view_t *view, int by);

/* Recalculates difference of two panes scroll positions. */
void update_scroll_bind_offset(void);

/* Layout related functions. */

/* Enables/disables ls-like style of the view. */
void fview_set_lsview(struct view_t *view, int enabled);

/* Checks whether view displays grid that's filled by column. */
int fview_is_transposed(const struct view_t *view);

/* Checks whether view currently displays the path within itself.  Returns
 * non-zero if so and zero otherwise. */
int fview_previews(struct view_t *view, const char path[]);

/* Enables/disables cascading columns style of the view. */
void fview_set_millerview(struct view_t *view, int enabled);

/* Requests update of view geometry properties (stuff that depends on
 * dimensions; there is also an implicit dependency on file list, because grid
 * is defined by longest file name). */
void fview_update_geometry(struct view_t *view);

/* Callback-like function which triggers some view-specific updates after
 * directory of the view changes. */
void fview_dir_updated(struct view_t *view);

/* Callback-like function which triggers some view-specific updates after list
 * of files changes. */
void fview_list_updated(struct view_t *view);

/* Callback-like function which triggers some view-specific updates after
 * decorations of files change. */
void fview_decors_updated(struct view_t *view);

/* Callback-like function which triggers some view-specific updates after cursor
 * position in the list changed. */
void fview_position_updated(struct view_t *view);

/* Callback-like function which triggers some view-specific updates after view
 * sorting changed. */
void fview_sorting_updated(struct view_t *view);

TSTATIC_DEFS(
	struct format_info_t;
	void format_name(void *data, size_t buf_len, char buf[],
		const struct format_info_t *info);
)

#endif /* VIFM__UI__FILEVIEW_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
