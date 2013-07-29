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

/* Makes buffer empty. */
void text_buffer_clear(void);

/* Adds the string to buffer. */
void text_buffer_add(const char msg[]);

/* Adds formated string to buffer. */
void text_buffer_addf(const char format[], ...);

/* Returns pointer to a read-only string, which may become invalid on any other
 * call of functions in this module.  Never returns NULL, only empty string. */
const char * text_buffer_get(void);

#endif /* VIFM__ENGINE__TEXT_BUFFER_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
