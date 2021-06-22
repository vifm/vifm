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

#ifndef VIFM__UTILS__UTILS_INT_H__
#define VIFM__UTILS__UTILS_INT_H__

#include "utils.h"

/* Executes an external command in shell without clearing up the screen.
 * Returns error code, which is zero on success. */
int run_in_shell_no_cls(char command[], ShellRequester by);

/* Same as run_in_shell_no_cls(), but provides the command with custom input.
 * Don't pass pipe for input, it can cause deadlock. */
int run_with_input(char command[], FILE *input, ShellRequester by);

#endif /* VIFM__UTILS__UTILS_INT_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
