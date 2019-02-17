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

#ifndef VIFM__UTILS__FILE_STREAMS_H__
#define VIFM__UTILS__FILE_STREAMS_H__

#include <stddef.h> /* size_t */
#include <stdio.h> /* FILE */

/* File stream reading related functions, that treat all eols (unix, mac, dos)
 * right. */

/* Reads line of dynamic size from the file stream.  The buffer can be NULL.
 * Trailing eol character is removed.  Returns reallocated buffer or NULL when
 * eof is reached and on not enough memory error (in this case the buffer is
 * freed by the function). */
char * read_line(FILE *fp, char buffer[]);

/* Reads line from file stream to buffer of predefined size, which should be at
 * least one character.  The bufsz parameters includes terminating null
 * character.  Newline character is preserved.  Returns value of the buf on
 * success, otherwise NULL is returned. */
char * get_line(FILE *fp, char buf[], size_t bufsz);

/* Skips file stream content until after BOM, if any.  Recovers only the last
 * read character. */
void skip_bom(FILE *fp);

#endif /* VIFM__UTILS__FILE_STREAMS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
