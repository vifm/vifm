/* vifm
 * Copyright (C) 2013 xaizek.
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

#ifndef VIFM__UI__ESCAPE_H__
#define VIFM__UI__ESCAPE_H__

#include <regex.h>

#include <curses.h>

#include <stddef.h> /* size_t */

#include "../utils/test_helpers.h"
#include "colors.h"

/* Possible modes of processing escape codes. */
typedef enum
{
	ESM_SHORT,          /* "Normal" state for standard escape codes. */
	ESM_GOT_FG_PREFIX,  /* After `^[38;`. */
	ESM_GOT_BG_PREFIX,  /* After `^[48;`. */
	ESM_WAIT_FG_COLOR,  /* After `^[38;5;`. */
	ESM_WAIT_BG_COLOR,  /* After `^[48;5;`. */
	ESM_WAIT_FG_R_COMP, /* After `^[38;2;`. */
	ESM_WAIT_FG_G_COMP, /* After `^[38;2;R;`. */
	ESM_WAIT_FG_B_COMP, /* After `^[38;2;G;`. */
	ESM_WAIT_BG_R_COMP, /* After `^[48;2;`. */
	ESM_WAIT_BG_G_COMP, /* After `^[48;2;R;`. */
	ESM_WAIT_BG_B_COMP, /* After `^[48;2;G;`. */
}
EscStateMode;

/* Holds state of escape sequence parsing. */
typedef struct esc_state
{
	EscStateMode mode;   /* Current mode of processing escape codes. */

	int attrs;           /* Current set of attributes. */
	int fg : 25;         /* Current foreground color. */
	int is_fg_direct: 2; /* Whether fg contains RGB data. */
	int bg : 25;         /* Current background color. */
	int is_bg_direct: 2; /* Whether bg contains RGB data. */

	col_attr_t defaults; /* Default values of other fields. */
	int max_colors;      /* Limit on number of colors. */
}
esc_state;

/* Returns a copy of the str with all escape sequences removed.  The string
 * returned should be freed by a caller. */
char * esc_remove(const char str[]);

/* Returns number of characters in the str taken by terminal escape
 * sequences. */
size_t esc_str_overhead(const char str[]);

/* Highlights all matches of the re regular expression in the line using color
 * inversion considering escape sequences in the line.  Returns processed string
 * that should be freed by the caller or NULL on error. */
char * esc_highlight_pattern(const char line[], const regex_t *re);

/* Prints at most whole line to a window with column and row initial offsets
 * while honoring maximum character positions specified by the max_width
 * parameter.  Sets *printed to number of screen characters actually printed,
 * it's set to zero otherwise.  The truncated parameter specifies whether
 * printed part is the final part of the line that's being printed.  Returns
 * offset in the line at which line processing was stopped. */
int esc_print_line(const char line[], WINDOW *win, int column, int row,
		int max_width, int dry_run, int truncated, esc_state *state, int *printed);

/* Initializes escape sequence parsing state with values from the defaults and
 * limit on number of colors. */
void esc_state_init(esc_state *state, const col_attr_t *defaults,
		int max_colors);

TSTATIC_DEFS(
	size_t get_char_width_esc(const char str[]);
	void esc_state_update(esc_state *state, const char str[]);
	const char * strchar2str(const char str[], int pos, size_t *screen_width);
)

#endif /* VIFM__UI__ESCAPE_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
