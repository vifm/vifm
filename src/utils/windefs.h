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

#ifndef VIFM__UTILS__WINDEFS_H__
#define VIFM__UTILS__WINDEFS_H__

#ifndef REQUIRED_WINVER
#error REQUIRED_WINVER macro not defined.
#endif

#ifndef WINVER
#define WINVER REQUIRED_WINVER
#elif WINVER < REQUIRED_WINVER
#error Version of Windows API is lower than required, expected it to \
       be >= REQUIRED_WINVER.
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT REQUIRED_WINVER
#elif _WIN32_WINNT < REQUIRED_WINVER
#error Version of Windows NT API is lower than required, expected it to \
       be >= REQUIRED_WINVER.
#endif

#endif /* VIFM__UTILS__WINDEFS_H__*/

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
