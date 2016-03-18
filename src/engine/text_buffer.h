/* vifm
 * Copyright (C) 2012 xaizek.
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

#ifndef VIFM__ENGINE__TEXT_BUFFER_H__
#define VIFM__ENGINE__TEXT_BUFFER_H__

#include "../utils/macros.h"

/* Typical usage examples:
 *
 * vle_textbuf *msg = vle_tb_create();
 * if(*msg != NULL)
 * {
 *   vle_tb_append_line(msg, "line1");
 *   ...
 *   vle_tb_append_line(msg, "lineN");
 *   output(vle_tb_get_data(msg));
 *   vle_tb_free(msg);
 * }
 *
 * or
 *
 * vle_textbuf *msg = vle_tb_create();
 * if(*msg != NULL)
 * {
 *   vle_tb_append_line(msg, "line1");
 *   ...
 *   vle_tb_append_line(msg, "lineN");
 *   return vle_tb_release(msg);
 * } */

/* Opaque text buffer type. */
typedef struct vle_textbuf vle_textbuf;

/* Predefined buffer for collecting errors of the engine. */
extern vle_textbuf *const vle_err;

/* Prepares the buffer for use.  Returns pointer to newly allocated text buffer
 * or NULL on memory allocation error. */
vle_textbuf * vle_tb_create(void);

/* Frees the buffer.  tb can be NULL. */
void vle_tb_free(vle_textbuf *tb);

/* Releases data from buffer possession and frees the buffer.  tb can't be
 * NULL.  Returns the data. */
char * vle_tb_release(vle_textbuf *tb);

/* Clears the buffer. */
void vle_tb_clear(vle_textbuf *tb);

/* Appends the string to specified buffer. */
void vle_tb_append(vle_textbuf *tb, const char str[]);

/* Appends formatted string to specified buffer. */
void vle_tb_appendf(vle_textbuf *tb, const char format[], ...)
	_gnuc_printf(2, 3);

/* Appends the string to specified buffer. */
void vle_tb_append(vle_textbuf *tb, const char str[]);

/* Appends the line (terminated with newline character) to specified buffer. */
void vle_tb_append_line(vle_textbuf *tb, const char str[]);

/* Appends formatted line (terminated with newline character) to specified
 * buffer. */
void vle_tb_append_linef(vle_textbuf *tb, const char format[], ...)
	_gnuc_printf(2, 3);

/* Returns pointer to a read-only string, which may become invalid on any other
 * call of functions in this module.  Never returns NULL, only empty string. */
const char * vle_tb_get_data(vle_textbuf *tb);

#endif /* VIFM__ENGINE__TEXT_BUFFER_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
