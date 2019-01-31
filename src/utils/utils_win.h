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

#include <windows.h>

#include <stddef.h> /* size_t */
#include <stdint.h> /* uint32_t */
#include <stdio.h> /* FILE */
#include <wchar.h> /* wchar_t */

#include "macros.h"
#include "test_helpers.h"

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

/* Executes a command (cmd) using CreateProcess() API function.  Expects path
 * that contain spaces to be enclosed in double quotes.  On internal error
 * returns last error code and sets *returned_exit_code to zero, otherwise sets
 * *returned_exit_code to non-zero and returns exit code of a process. */
int win_exec_cmd(char cmd[], int *returned_exit_code);

/* Turns command into the one suitable to be run by a shell.  Returns the
 * string. */
char * win_make_sh_cmd(const char cmd[], ShellRequester by);

int is_win_executable(const char name[]);

int is_vista_and_above(void);

/* Converts Windows attributes to a string.
 * Returns pointer to a statically allocated buffer. */
const char * attr_str(uint32_t attr);

/* Converts Windows attributes to a long string containing all attribute values.
 * Returns pointer to a statically allocated buffer. */
const char * attr_str_long(uint32_t attr);

/* Returns pointer to a statically allocated buffer. */
const char * escape_for_cd(const char str[]);

/* tmpfile() for Windows, the way it should have been implemented.  Returns file
 * handler opened for read and write that is automatically removed on
 * application close.  Don't use tmpfile(), they utterly failed to implement
 * it. */
FILE * win_tmpfile();

/* Tries to cancel process gracefully.  Returns zero if cancellation was
 * requested, otherwise non-zero is returned. */
int win_cancel_process(DWORD pid, HANDLE hprocess);

/* Converts FILETIME to time_t.  Returns converted time. */
time_t win_to_unix_time(FILETIME ft);

TSTATIC_DEFS(
	int should_wait_for_program(const char cmd[]);
)

#endif /* VIFM__UTILS__UTILS_WIN_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
