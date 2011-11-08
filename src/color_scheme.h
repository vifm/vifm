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

#ifndef __COLOR_SCHEME_H__
#define __COLOR_SCHEME_H__

#include <limits.h>

#if !defined(NAME_MAX) && defined(_WIN32)
#include <io.h>
#define NAME_MAX FILENAME_MAX
#endif

#define MAX_COLOR_SCHEMES 8

enum
{
	WIN_COLOR,
	DIRECTORY_COLOR,
	LINK_COLOR,
	BROKEN_LINK_COLOR,
	SOCKET_COLOR,
	DEVICE_COLOR,
	FIFO_COLOR,
	EXECUTABLE_COLOR,
	SELECTED_COLOR,
	CURR_LINE_COLOR,
	TOP_LINE_COLOR,
	TOP_LINE_SEL_COLOR,
	STATUS_LINE_COLOR,
	MENU_COLOR,
	CMD_LINE_COLOR,
	ERROR_MSG_COLOR,
	BORDER_COLOR,
	CURRENT_COLOR,      /* for internal use only */
	MENU_CURRENT_COLOR, /* for internal use only */
	MAXNUM_COLOR
};

enum
{
	DCOLOR_BASE = 1,
	LCOLOR_BASE = DCOLOR_BASE + MAXNUM_COLOR,
	RCOLOR_BASE = LCOLOR_BASE + MAXNUM_COLOR,
};

typedef struct
{
	int fg;
	int bg;
	int attr;
}col_attr_t;

typedef struct
{
	char name[NAME_MAX];
	char dir[PATH_MAX];
	int defaulted;
	col_attr_t color[MAXNUM_COLOR];
}col_scheme_t;

extern char *HI_GROUPS[];
extern char *COLOR_NAMES[8];

/* directory should be NULL if you want to set default directory */
void load_color_scheme_colors(void);
void load_def_scheme(void);
int check_directory_for_color_scheme(int left, const char *dir);
/* Returns value lower than zero when nothing is found */
int find_color_scheme(const char *name);
void complete_colorschemes(const char *name);
const char * attrs_to_str(int attrs);
void check_color_scheme(col_scheme_t *cs);
void assoc_dir(const char *name, const char *dir);
void write_color_scheme_file(void);
void mix_colors(col_attr_t *base, const col_attr_t *mixup);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
