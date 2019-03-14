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

#ifndef VIFM__UI__QUICKVIEW_H__
#define VIFM__UI__QUICKVIEW_H__

#include <stddef.h> /* size_t */
#include <stdio.h> /* FILE */

#include "../utils/test_helpers.h"
#include "colors.h"

struct dir_entry_t;
struct view_t;

/* Description of area used for preview. */
typedef struct
{
	struct view_t *source; /* View which does the preview. */
	struct view_t *view;   /* View which displays the preview. */
	col_attr_t def_col;    /* Default color of the area. */
	int x;                 /* Relative x coordinate of the top-left corner. */
	int y;                 /* Relative y coordinate of the top-left corner. */
	int w;                 /* Width of the area. */
	int h;                 /* Height of the area. */
}
preview_area_t;

/* Enables quick view (just enables, no drawing) if possible.  Returns zero on
 * success, otherwise non-zero is returned and error message is printed on the
 * statusbar.*/
int qv_ensure_is_shown(void);

/* Checks whether quick view can be shown.  Returns non-zero if so, otherwise
 * zero is returned and error message is printed on the statusbar. */
int qv_can_show(void);

/* Draws current file of the view in other view.  Does nothing if drawing
 * doesn't make sense (e.g. only one pane is visible). */
void qv_draw(struct view_t *view);

/* Draws file entry on an area. */
void qv_draw_on(const struct dir_entry_t *entry, const preview_area_t *parea);

/* Toggles state of the quick view. */
void qv_toggle(void);

/* Quits preview pane or view modes. */
void qv_hide(void);

/* Expands and executes viewer command.  Returns file containing results of the
 * viewer. */
FILE * qv_execute_viewer(const char viewer[]);

/* Performs view clearing with the given command, which can be NULL in which
 * case only internal clearing is done. */
void qv_cleanup(struct view_t *view, const char cmd[]);

/* Gets viewer command for a file considering its type (directory vs. file).
 * Returns NULL if no suitable viewer available, otherwise returns pointer to
 * string stored internally. */
const char * qv_get_viewer(const char path[]);

/* Previews directory, actual preview is to be read from returned stream.
 * Returns the stream or NULL on error. */
FILE * qv_view_dir(const char path[]);

/* Decides on path that should be explored when cursor points to the given
 * entry. */
void qv_get_path_to_explore(const struct dir_entry_t *entry, char buf[],
		size_t buf_len);

/* Informs this unit that it's data was probably erased from the screen. */
void qv_ui_updated(void);

TSTATIC_DEFS(
	struct strlist_t;
	struct strlist_t read_lines(FILE *fp, int max_lines);
)

#endif /* VIFM__UI__QUICKVIEW_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
