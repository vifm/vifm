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

#ifndef __FILE_STREAMS_H__
#define __FILE_STREAMS_H__

#include <stddef.h> /* size_t */
#include <stdio.h> /* FILE */

/* File stream reading related functions, that treat all eols (unix, mac, dos)
 * right. */

/* Reads line from file stream. */
char * get_line(FILE *fp, char *buf, size_t bufsz);
/* Skips file stream content until and including eol character. */
void skip_until_eol(FILE *fp);
/* Removes eol symbols from file stream. */
void remove_eol(FILE *fp);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
