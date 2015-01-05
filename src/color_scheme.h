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

/* Pseudo name of the default built-in color scheme. */
#define DEF_CS_NAME "<built-in default>"

/* State of a color scheme. */
typedef enum
{
	CSS_NORMAL,    /* Color scheme is ready to be used. */
	CSS_DEFAULTED, /* Color scheme was defaulted to have builtin values. */
	CSS_LOADING,   /* Color scheme is being constructed at the moment. */
	CSS_BROKEN,    /* Color scheme is broken and should be defaulted. */
}
ColorSchemeState;

typedef struct
{
	char name[NAME_MAX];            /* Name of the color scheme. */
	char dir[PATH_MAX];             /* Associated root dir of the color scheme. */
	ColorSchemeState state;         /* Current state. */
	col_attr_t color[MAXNUM_COLOR]; /* Colors with their attributes. */
	int pair[MAXNUM_COLOR];         /* Pairs for corresponding color. */
}
col_scheme_t;

extern char *HI_GROUPS[];
extern char *LIGHT_COLOR_NAMES[8];
extern char *XTERM256_COLOR_NAMES[256];

/* Loads primary color scheme specified by the name.  Returns new value for
 * curr_stats.save_msg. */
int load_primary_color_scheme(const char name[]);

/* Loads configured color scheme color pairs, so they actually visible on a
 * screen. */
void load_color_scheme_colors(void);

void load_def_scheme(void);

/* Resets color scheme to default builtin values and reloads them. */
void reset_color_scheme(col_scheme_t *cs);

/* Copies data from *from to *to. */
void assign_color_scheme(col_scheme_t *to, const col_scheme_t *from);

/* The color scheme with the longest matching directory path is the one that
 * is chosen.  Return non-zero if non-default colorscheme should be used for the
 * specified directory, otherwise zero is returned. */
int check_directory_for_color_scheme(int left, const char dir[]);

/* Lists names of all color schemes.  Allocates an array of strings, which
 * should be freed by the caller.  Always sets *len.  Returns NULL on error. */
char ** list_color_schemes(int *len);

/* Returns non-zero if colorscheme named name exists. */
int color_scheme_exists(const char name[]);

void complete_colorschemes(const char name[]);

const char * attrs_to_str(int attrs);

void check_color_scheme(col_scheme_t *cs);

void assoc_dir(const char *name, const char *dir);

/* Aborts if color schemes directory exists, otherwise creates one containing
 * "Default" color scheme. */
void write_color_scheme_file(void);

/* Converts color specified by an integer to a string and writes result in a
 * buffer of length buf_len pointed to by str_buf. */
void color_to_str(int color, size_t buf_len, char str_buf[]);

void mix_colors(col_attr_t *base, const col_attr_t *mixup);

/* Checks that color is non-empty (e.g. set from outside).  Returns non-zero if
 * so, otherwise zero is returned. */
int is_color_set(const col_attr_t *color);

#endif /* VIFM__COLOR_SCHEME_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
