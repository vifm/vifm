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
#include "utils.h"

#include <sys/types.h> /* gid_t mode_t pid_t uid_t */
#include <sys/wait.h> /* WEXITSTATUS() WIFEXITED() */

#define PAUSE_CMD "vifm-pause"
#define PAUSE_STR "; "PAUSE_CMD

struct cancellation_t;

/* Checks whether cancelling of current operation is requested and sends SIGINT
 * to process specified by its process id to request cancellation. */
void process_cancel_request(pid_t pid,
		const struct cancellation_t *cancellation);

/* Waits for a process to finish and queries for its exit status.  Returns exit
 * status of the process specified by its identifier. */
int get_proc_exit_status(pid_t pid);

/* If err_only then use stderr and close stdin and stdout, otherwise both stdout
 * and stderr are redirected to the pipe.  Non-zero preserve_stdin prevents
 * stdin from being bound to /dev/null. */
void _gnuc_noreturn run_from_fork(int pipe[2], int err_only, int preserve_stdin,
		char cmd[], ShellRequester by);

/* Frees some resources before exec(), which shouldn't be inherited and remain
 * allocated in child process or it might make those resources appear busy
 * (e.g., pipe not being closed, directory being still in use). */
void prepare_for_exec(void);

/* Extracts name of the shell to be used with execv*() function.  Returns
 * pointer to statically allocated buffer. */
char * get_execv_path(char shell[]);

/* Creates array to be passed into one of execv*() functions.  To be used by
 * forked process.  Returns newly allocated array with some strings allocated,
 * some as is.  Memory management shouldn't matter at this point, we either
 * successfully replace process image or terminate. */
char ** make_execv_array(char shell[], char shell_flag[], char cmd[]);

/* Converts the mode to string representation of permissions. */
void get_perm_string(char buf[], int len, mode_t mode);

/* Maps string with user id as a string or user name to integer id.  Returns
 * zero on success and non-zero otherwise. */
int get_uid(const char user[], uid_t *uid);

/* Maps string with group id as a string or group name to integer id.  Returns
 * zero on success and non-zero otherwise. */
int get_gid(const char group[], gid_t *gid);

/* Converts status to exit code.  Input can be -1, meaning that status is
 * unknown.  Returns the exit code or -1 for -1 status. */
int status_to_exit_code(int status);

int S_ISEXE(mode_t mode);

#endif /* VIFM__UTILS__UTILS_NIX_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
