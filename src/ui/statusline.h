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

#include "ui.h"

#include "../utils/test_helpers.h"

/* Status line managing.  Job bar is considered as a continuation of status bar,
 * but its visibility is controlled separately. */

void update_stat_window(FileView *view);

/* Puts status line where it's suppose to be according to other elements (also
 * moves job bar).  Does nothing if displaying status line is disabled.  Returns
 * non-zero if status line is visible, and zero otherwise. */
int ui_stat_reposition(int statusbar_height);

/* Updates content of status line on the screen (also updates job bar). */
void ui_stat_refresh(void);

/* Gets height of the job bar (>= 0).  Returns the height. */
int ui_stat_job_bar_height(void);

TSTATIC_DEFS(
	char * expand_status_line_macros(FileView *view, const char format[]);
)

#endif /* VIFM__UI__STATUSLINE_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
