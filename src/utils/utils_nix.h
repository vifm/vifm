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

#ifndef VIFM__UTILS__UTILS_NIX_H__
#define VIFM__UTILS__UTILS_NIX_H__

#include "macros.h"

#include <sys/types.h> /* gid_t mode_t uid_t */
#include <sys/wait.h> /* WEXITSTATUS() WIFEXITED() */

#define PAUSE_CMD "vifm-pause"
#define PAUSE_STR "; "PAUSE_CMD

void _gnuc_noreturn run_from_fork(int pipe[2], int err, char *cmd);

/* Converts the mode to string representation of permissions. */
void get_perm_string(char buf[], int len, mode_t mode);

int get_uid(const char user[], uid_t *uid);

int get_gid(const char group[], gid_t *gid);

int S_ISEXE(mode_t mode);

#endif /* VIFM__UTILS__UTILS_NIX_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
