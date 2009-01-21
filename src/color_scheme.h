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

#include<limits.h>
#define MAXNUM_COLOR 12

#define MENU_COLOR 0
#define BORDER_COLOR 1
#define WIN_COLOR 2
#define	STATUS_BAR_COLOR 3
#define CURR_LINE_COLOR 4
#define DIRECTORY_COLOR 5
#define LINK_COLOR 6
#define SOCKET_COLOR 7
#define DEVICE_COLOR 8
#define EXECUTABLE_COLOR 9
#define SELECTED_COLOR 10
#define CURRENT_COLOR 11

typedef struct _Col_attr {
	int name;
	int fg;
	int bg;
} Col_attr;


typedef struct _Col_Scheme {
	char name[NAME_MAX];
	char dir[PATH_MAX];
	Col_attr color[12];
} Col_scheme;

extern Col_scheme *col_schemes;

void read_color_scheme_file();
void write_color_scheme_file();
int check_directory_for_color_scheme(const char *);
void load_color_scheme(char  *name, char *dir);

#endif
