/* vifm
 * Copyright (C) 2020 xaizek.
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

#ifndef VIFM__UI__COLORED_LINE_H__
#define VIFM__UI__COLORED_LINE_H__

#include <curses.h> /* WINDOW cchar_t */

#include <stddef.h> /* size_t */

/* This unit provides a type that bundles line with attributes.  Attributes are
 * stored in a parallel array of characters.  It contains a character in the
 * set [0-9 ] (space included) per screen position of the UTF-8 line.  Each
 * attribute character specifies which user highlight group should be used
 * starting with that offset on the screen. */

/* Line paired with parallel array of character that specify user colors. */
typedef struct cline_t
{
	char *line;       /* Text of the line. */
	size_t line_len;  /* Length of line field. */
	char *attrs;      /* Specifies when to enable which user highlight group. */
	size_t attrs_len; /* Length of attrs field. */
}
cline_t;

/* Creates an empty colored line. */
cline_t cline_make(void);

/* Makes sure that cline->attrs has at least as many elements as cline->line
 * contains screen positions for its UTF-8 characters + extra_width.  Returns
 * non-zero if cline->attrs has extra characters compared to cline->line. */
int cline_sync(cline_t *cline, int extra_width);

/* Sets attribute to be used for text appended later on. */
void cline_set_attr(cline_t *cline, char attr);

/* Makes cline empty. */
void cline_clear(cline_t *cline);

/* Finalizes cline by synchronizing and truncating it to make contents and
 * attributes match each other. */
void cline_finish(cline_t *cline);

/* Appends attributes from admixture freeing them (but not the line). */
void cline_splice_attrs(cline_t *cline, cline_t *admixture);

struct col_attr_t;

/* Prints colored line onto a window highlighting it according to the
 * attributes. */
void cline_print(const cline_t *cline, WINDOW *win,
		const struct col_attr_t *def_col);

/* Ensures that cline is of width (in character positions) less than or equal to
 * max_width and is right aligned putting ellipsis on the left side if
 * needed. */
void cline_left_ellipsis(cline_t *cline, size_t max_width, const char ell[]);

/* Frees all resources allocated by cline.  The parameter can't be NULL. */
void cline_dispose(cline_t *cline);

#endif /* VIFM__UI__COLORED_LINE_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
