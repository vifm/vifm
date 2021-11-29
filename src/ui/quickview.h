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

#include "../macros.h"
#include "colors.h"

struct dir_entry_t;
struct view_t;

/* Description of area used for preview. */
typedef struct preview_area_t preview_area_t;
struct preview_area_t
{
	struct view_t *source; /* View which does the preview. */
	struct view_t *view;   /* View which displays the preview. */
	col_attr_t def_col;    /* Default color of the area. */
	int x;                 /* Relative x coordinate of the top-left corner. */
	int y;                 /* Relative y coordinate of the top-left corner. */
	int w;                 /* Width of the area. */
	int h;                 /* Height of the area. */
};

/* Enables quick view (just enables, no drawing) if possible.  Returns zero on
 * success, otherwise non-zero is returned and error message is printed on the
 * status bar.*/
int qv_ensure_is_shown(void);

/* Checks whether quick view can be shown.  Returns non-zero if so, otherwise
 * zero is returned and error message is printed on the status bar. */
int qv_can_show(void);

/* Draws current file of the view in other view.  Does nothing if drawing
 * doesn't make sense (e.g. only one pane is visible). */
void qv_draw(struct view_t *view);

/* Draws file entry on an area.  Returns preview clear command or NULL. */
const char * qv_draw_on(const struct dir_entry_t *entry,
		const preview_area_t *parea);

/* Toggles state of the quick view. */
void qv_toggle(void);

/* Quits preview pane or view modes. */
void qv_hide(void);

/* Expands viewer for the view.  The flags parameter can be NULL.  Returns a
 * pointer to newly allocated memory, which should be released by the caller. */
char * qv_expand_viewer(struct view_t *view, const char viewer[],
		MacroFlags *flags);

/* Performs view clearing with the given command, which can be NULL in which
 * case only internal clearing is done. */
void qv_cleanup(struct view_t *view, const char cmd[]);

/* Erases area using external command if cmd parameter isn't NULL. */
void qv_cleanup_area(const preview_area_t *parea, const char cmd[]);

/* Gets viewer command for a file considering its type (directory vs. file).
 * Returns NULL if no suitable viewer available, otherwise returns pointer to
 * string stored internally. */
const char * qv_get_viewer(const char path[]);

/* Previews directory, actual preview is to be read from returned stream.
 * Returns the stream or NULL on error. */
FILE * qv_view_dir(const char path[], int max_lines);

/* Decides on path that should be explored when cursor points to the given
 * entry. */
void qv_get_path_to_explore(const struct dir_entry_t *entry, char buf[],
		size_t buf_len);

/* Informs this unit that it's data was probably erased from the screen. */
void qv_ui_updated(void);

#endif /* VIFM__UI__QUICKVIEW_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
