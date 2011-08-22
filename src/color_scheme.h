/* vifm
 * Copyright (C) 2001 Ken Steen.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifndef __COLOR_SCHEME_H__
#define __COLOR_SCHEME_H__

#include <limits.h>

enum Ui_colors {
	MENU_COLOR,
	BORDER_COLOR,
	WIN_COLOR,
	STATUS_BAR_COLOR,
	CURR_LINE_COLOR,
	DIRECTORY_COLOR,
	LINK_COLOR,
	SOCKET_COLOR,
	DEVICE_COLOR,
	EXECUTABLE_COLOR,
	SELECTED_COLOR,
	CURRENT_COLOR,
	BROKEN_LINK_COLOR,
	TOP_LINE_COLOR,
	STATUS_LINE_COLOR,
	MENU_CURRENT_COLOR, /* for internal use only */
	MAXNUM_COLOR
};

typedef struct _Col_attr {
	int fg;
	int bg;
} Col_attr;

typedef struct _Col_Scheme {
	char name[NAME_MAX];
	char dir[PATH_MAX];
	int defaulted;
	Col_attr color[MAXNUM_COLOR];
} Col_scheme;

extern Col_scheme *col_schemes;

void check_color_schemes(void);
void read_color_scheme_file(void);
int check_directory_for_color_scheme(const char *);
int find_color_scheme(const char *name);
void complete_colorschemes(const char *name);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
