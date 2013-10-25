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

#ifndef VIFM__UTILS__UTILS_H__
#define VIFM__UTILS__UTILS_H__

#include <regex.h>

#include <sys/types.h> /* gid_t mode_t uid_t */

#include <stddef.h> /* size_t */
#include <stdint.h> /* uint64_t */
#include <wchar.h> /* wchar_t */

/* Regular expressions. */

/* Gets flags for compiling a regular expression specified by the pattern taking
 * 'ignorecase' and 'smartcase' options into account.  Returns regex flags. */
int get_regexp_cflags(const char pattern[]);
/* Decides whether case should be ignored for the pattern.  Considers
 * 'ignorecase' and 'smartcase' options.  Returns non-zero when case should be
 * ignored, otherwise zero is returned. */
int regexp_should_ignore_case(const char pattern[]);
const char * get_regexp_error(int err, regex_t *re);

/* Shell and program running. */

/* Executes an external command.  Clears the screen up on Windows before running
 * the command.  Returns error code, which is zero on success. */
int my_system(char command[]);
/* Pauses shell.  Assumes that curses interface is off. */
void pause_shell(void);
/* Called after return from a shellout to provide point to recover UI. */
void recover_after_shellout(void);

/* Other functions. */

int is_on_slow_fs(const char full_path[]);
/* Fills supplied buffer with user friendly representation of file size.
 * Returns non-zero in case resulting string is a shortened variant of size. */
int friendly_size_notation(uint64_t num, int str_size, char *str);
const char * enclose_in_dquotes(const char *str);
int my_chdir(const char *path);
/* Expands all environment variables and tilde in the path.  Allocates
 * memory, that should be freed by the caller. */
char * expand_path(const char path[]);
/* Expands all environment variables in the str of form "$envvar".  Non-zero
 * escape_vals means escaping suitable for internal use.  Allocates and returns
 * memory that should be freed by the caller. */
char * expand_envvars(const char str[], int escape_vals);
/* Makes filename unique by adding an unique suffix to it.
 * Returns pointer to a statically allocated buffer */
const char * make_name_unique(const char filename[]);
/* Returns process identification in a portable way. */
unsigned int get_pid(void);
/* Finds command name in the command line and writes it to the buf.
 * Raw mode will preserve quotes on Windows.
 * Returns a pointer to the argument list. */
char * extract_cmd_name(const char line[], int raw, size_t buf_len, char buf[]);
/* Determines columns needed for a wide character.  Returns number of columns,
 * on error default value of 1 is returned. */
int vifm_wcwidth(wchar_t c);

#ifdef _WIN32
#include "utils_win.h"
#else
#include "utils_nix.h"
#endif

#endif /* VIFM__UTILS__UTILS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
