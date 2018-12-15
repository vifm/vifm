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

#ifndef VIFM__UI__COLOR_SCHEME_H__
#define VIFM__UI__COLOR_SCHEME_H__

#include <regex.h> /* regex_t */

#include <stddef.h> /* size_t */

#include "../compat/fs_limits.h"
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

struct matchers_t;

/* Single file highlight description. */
typedef struct
{
	struct matchers_t *matchers; /* Name matcher object. */
	col_attr_t hi;               /* File appearance parameters. */
}
file_hi_t;

/* Color scheme description. */
typedef struct
{
	char name[NAME_MAX + 1];        /* Name of the color scheme. */
	char dir[PATH_MAX + 1];         /* Associated root dir of the color scheme. */
	ColorSchemeState state;         /* Current state. */
	col_attr_t color[MAXNUM_COLOR]; /* Colors with their attributes. */
	int pair[MAXNUM_COLOR];         /* Pairs for corresponding color. */

	file_hi_t *file_hi; /* List of file highlight preferences. */
	int file_hi_count;  /* Number of file highlight definitions. */
}
col_scheme_t;

/* Names of highlight groups.  Contains MAXNUM_COLOR elements. */
extern char *HI_GROUPS[];
/* Descriptions of highlight groups.  Contains MAXNUM_COLOR elements. */
extern const char *HI_GROUPS_DESCR[];
/* Names of light versions of standard colors. */
extern char *LIGHT_COLOR_NAMES[8];
/* Names from 256-color palette. */
extern char *XTERM256_COLOR_NAMES[256];

/* Loads primary color scheme specified by the name. */
void cs_load_primary(const char name[]);

/* Loads the first color scheme in the order given that exists and is supported
 * by the terminal.  If none matches, current one remains unchanged. */
void cs_load_primary_list(char *names[], int count);

/* Loads color pairs of all active color schemes, so the colors are actually
 * visible on the screen. */
void cs_load_pairs(void);

/* Resets all color schemes and everything related to default (or empty)
 * state. */
void cs_load_defaults(void);

/* Resets color scheme to default builtin values and reloads them (color
 * pairs). */
void cs_reset(col_scheme_t *cs);

/* Copies data from *from to *to. */
void cs_assign(col_scheme_t *to, const col_scheme_t *from);

/* The color scheme with the longest matching directory path is the one that
 * is chosen.  Left chooses the left pane over the right one.  Return non-zero
 * if non-default color scheme should be used for the specified directory,
 * otherwise zero is returned. */
int cs_load_local(int left, const char dir[]);

/* Checks whether local color schemes do not have file extensions.  Returns
 * non-zero if so, otherwise zero is returned. */
int cs_have_no_extensions(void);

/* Adds ".vifm" extension to color scheme files as per new format. */
void cs_rename_all(void);

/* Lists names of all color schemes.  Allocates an array of strings, which
 * should be freed by the caller.  Always sets *len.  Returns NULL on error. */
char ** cs_list(int *len);

/* Checks existence of the specified color scheme.  Returns non-zero if color
 * scheme named name exists, otherwise zero is returned. */
int cs_exists(const char name[]);

/* Performs completion of color names. */
void cs_complete(const char name[]);

/* Converts set of attributes into a string.  Returns pointer to a statically
 * allocated buffer. */
const char * cs_attrs_to_str(int attrs);

/* Associates color scheme specified by its name with the given path. */
void cs_assoc_dir(const char name[], const char dir[]);

/* Does nothing if color schemes directory exists, otherwise creates one
 * containing "Default" color scheme. */
void cs_write(void);

/* Converts color specified by an integer to a string and writes result in a
 * buffer of length buf_len pointed to by str_buf. */
void cs_color_to_str(int color, size_t buf_len, char str_buf[]);

/* Mixes colors of *mixup into the base color.  Non-transparent properties of
 * *mixup are transferred onto *base. */
void cs_mix_colors(col_attr_t *base, const col_attr_t *mixup);

/* Registers pattern-highlight pair for active color scheme.  Reports memory
 * error to the user. */
void cs_add_file_hi(struct matchers_t *matchers, const col_attr_t *hi);

/* Gets filename-specific highlight.  hi_hint can't be NULL and should be equal
 * to -1 initially.  Returns NULL if nothing is found, otherwise returns pointer
 * to one of color scheme's highlights. */
const col_attr_t * cs_get_file_hi(const col_scheme_t *cs, const char fname[],
		int *hi_hint);

/* Removes filename-specific highlight by its pattern.  Returns non-zero on
 * successful removal and zero if pattern wasn't found. */
int cs_del_file_hi(const char matchers_expr[]);

/* Checks that color is non-empty (i.e. has at least one property set).  Returns
 * non-zero if so, otherwise zero is returned. */
int cs_is_color_set(const col_attr_t *color);

#endif /* VIFM__UI__COLOR_SCHEME_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
