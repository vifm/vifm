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

#ifndef VIFM__UTILS__COLORED_LINE_H__
#define VIFM__UTILS__COLORED_LINE_H__

#include <stddef.h> /* size_t */

/* Line paired with parallel array of character that specify user colors. */
typedef struct
{
	char *line;       /* Text of the line. */
	size_t line_len;  /* Length of line field. */
	char *attrs;      /* Specifies when to enable which user highlight group. */
	size_t attrs_len; /* Length of attrs field. */
}
cline_t;

/* Makes sure that cline->attrs has at least as many elements as cline->line
 * contains UTF-8 characters + extra_width.  Returns non-zero if cline->attrs
 * has extra characters compared to cline->line. */
int cline_sync(cline_t *cline, int extra_width);

#endif /* VIFM__UTILS__COLORED_LINE_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
