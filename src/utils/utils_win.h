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

#ifndef VIFM__UTILS__UTILS_WIN_H__
#define VIFM__UTILS__UTILS_WIN_H__

#include <windef.h>

#include <stddef.h> /* size_t */
#include <wchar.h> /* wchar_t */

#include "macros.h"

#ifndef WEXITSTATUS
#define WEXITSTATUS(a) (a)
#endif

#ifndef WIFEXITED
#define WIFEXITED(a) 1
#endif

#define lstat stat

#define PAUSE_CMD "vifm-pause"
#define PAUSE_STR " && pause || pause"

int wcwidth(wchar_t c);

int wcswidth(const wchar_t str[], size_t max_len);

/* Executes a command (cmd) using CreateProcess() API function.  On internal
 * error returns last error code and sets *returned_exit_code to zero, otherwise
 * sets *returned_exit_code to non-zero and returns exit code of a process. */
int win_exec_cmd(char cmd[], int *const returned_exit_code);

/* Checks executable existence trying to add executable extensions if needed. */
int win_executable_exists(const char path[]);

int is_win_executable(const char name[]);

int is_vista_and_above(void);

/* Converts Windows attributes to a string.
 * Returns pointer to a statically allocated buffer */
const char * attr_str(DWORD attr);

/* Converts Windows attributes to a long string containing all attribute values.
 * Returns pointer to a statically allocated buffer */
const char * attr_str_long(DWORD attr);

#endif /* VIFM__UTILS__UTILS_WIN_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
