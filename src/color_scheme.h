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

#ifndef VIFM__COLOR_SCHEME_H__
#define VIFM__COLOR_SCHEME_H__

#include <stddef.h> /* size_t */

#include "utils/fs_limits.h"
#ifdef _WIN32
#include "utils/utils.h"
#endif
#include "colors.h"

typedef struct
{
	char name[NAME_MAX];
	char dir[PATH_MAX];
	int defaulted;
	col_attr_t color[MAXNUM_COLOR];
}col_scheme_t;

extern char *HI_GROUPS[];
extern char *COLOR_NAMES[8];
extern char *LIGHT_COLOR_NAMES[8];

/* directory should be NULL if you want to set default directory */
void load_color_scheme_colors(void);
void load_def_scheme(void);
int check_directory_for_color_scheme(int left, const char *dir);
/* Lists names of all color schemes.  Allocates an array of strings, which
 * should be freed by the caller.  Always sets *len.  Returns NULL on error. */
char ** list_color_schemes(int *len);
/* Returns non-zero if colorscheme named name exists. */
int color_scheme_exists(const char name[]);
void complete_colorschemes(const char name[]);
const char * attrs_to_str(int attrs);
void check_color_scheme(col_scheme_t *cs);
void assoc_dir(const char *name, const char *dir);
void write_color_scheme_file(void);
/* Converts color specified by an integer to a string and writes result in a
 * buffer of length buf_len pointed to by str_buf. */
void color_to_str(int color, size_t buf_len, char str_buf[]);
void mix_colors(col_attr_t *base, const col_attr_t *mixup);

#endif /* VIFM__COLOR_SCHEME_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
