/* vifm
 * Copyright (C) 2014 xaizek.
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

#ifndef VIFM__UI__STATUSLINE_H__
#define VIFM__UI__STATUSLINE_H__

#include <curses.h> /* WINDOW */

#include <stddef.h> /* size_t */

#include "../utils/test_helpers.h"

struct view_t;

/* Status line managing.  Job bar is considered as a continuation of status bar,
 * but its visibility is controlled separately. */

/* Redraw contents of stat line (possibly lazily). */
void ui_stat_update(struct view_t *view, int lazy_redraw);

/* Puts status line where it's suppose to be according to other elements (also
 * moves job bar).  If displaying status line is disabled, force flag can help
 * ignore it for this call.  Returns non-zero if status line is visible, and
 * zero otherwise. */
int ui_stat_reposition(int statusbar_height, int force_stat_win);

/* Retrieves height of status line message.  Returns the height. */
int ui_stat_height(void);

/* Updates content of status line on the screen (also updates job bar). */
void ui_stat_refresh(void);

/* Gets height of the job bar (>= 0).  Returns the height. */
int ui_stat_job_bar_height(void);

struct bg_op_t;

/* Adds job to the bar. */
void ui_stat_job_bar_add(struct bg_op_t *bg_op);

/* Removes job from the bar. */
void ui_stat_job_bar_remove(struct bg_op_t *bg_op);

/* Informs about changes of the job. */
void ui_stat_job_bar_changed(struct bg_op_t *bg_op);

/* Fills job bar UI element with up-to-date content. */
void ui_stat_job_bar_redraw(void);

/* Checks for previously reported changes in background jobs and updates view if
 * needed. */
void ui_stat_job_bar_check_for_updates(void);

/* Draws single popup line with text and its description.  Attributes and cursor
 * should be managed outside this function.  max_width is the maximum width of
 * item, and is used to choose formatting style. */
void ui_stat_draw_popup_line(WINDOW *win, const char item[], const char descr[],
		size_t max_width);

#ifdef TEST
#include "colored_line.h"
#endif
TSTATIC_DEFS(
	cline_t expand_status_line_macros(struct view_t *view, const char format[]);
	char * find_view_macro(const char **format, const char macros[], char macro,
		int opt);
)

#endif /* VIFM__UI__STATUSLINE_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
