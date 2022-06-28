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

#include <sys/stat.h> /* stat */
#include <sys/types.h> /* gid_t mode_t uid_t */

#include <stddef.h> /* size_t wchar_t */
#include <stdint.h> /* uint64_t */
#include <stdio.h> /* FILE */
#include <time.h> /* time_t */

#include "../macros.h"
#include "../status.h"

/* Type of operating environment in which the application is running. */
typedef enum
{
	ET_UNIX, /* *nix-like environment (including cygwin). */
	ET_WIN,  /* Runs on Windows. */
}
EnvType;

/* Whether user customizations on how to invoke the shell should be applied. */
typedef enum
{
	SHELL_BY_APP,  /* Shell invocation originated internally. */
	SHELL_BY_USER, /* Shell invocation is requested by the user. */
}
ShellRequester;

/* Fine tuning of expand_envvars() behaviour. */
typedef enum
{
	EEF_NONE            = 0x00, /* None of the other options. */
	EEF_ESCAPE_VALS     = 0x01, /* Escape value suitable for internal use. */
	EEF_KEEP_ESCAPES    = 0x02, /* Only remove \ in front of $ and keep others. */
	EEF_DOUBLE_PERCENTS = 0x04, /* Double percents to make the resulting string
	                               suitable for further macro expansion. */
}
ExpandEnvFlags;

/* Defines an integer interval. */
typedef struct
{
	int first; /* Left inclusive bound. */
	int last;  /* Right inclusive bound. */
}
interval_t;

/* Forward declarations. */
struct dir_entry_t;
struct view_t;

/* Callback for process_cmd_output() function. */
typedef void (*cmd_output_handler)(const char line[], void *arg);

/* Shell and program running. */

/* Executes an external command.  Clears the screen up on Windows before running
 * the command.  Returns error code, which is zero on success. */
int vifm_system(char command[], ShellRequester by);

/* Same as vifm_system(), but provides the command with custom input.  Don't
 * pass pipe for input, it can cause deadlock. */
int vifm_system_input(char command[], FILE *input, ShellRequester by);

/* Pauses shell.  Assumes that curses interface is off. */
void pause_shell(void);

/* Called after return from a shellout to provide point to recover UI. */
void recover_after_shellout(void);

/* Invokes handler for each line read from stdout of the command specified via
 * cmd.  Input is redirected only if in parameter isn't NULL.  Don't pass pipe
 * for input, it can cause deadlock.  Error stream is displayed separately.
 * Implements heuristic according to which if command output includes null
 * character, it's taken as a separator instead of regular newline characters.
 * Supports cancellation.  Ignores exit code of the command and succeeds even if
 * it doesn't exist.  Returns zero on success, otherwise non-zero is
 * returned. */
int process_cmd_output(const char descr[], const char cmd[], FILE *input,
		int user_sh, int interactive, cmd_output_handler handler, void *arg);

/* Other functions. */

struct mntent;

/* Client of the traverse_mount_points() function.  Should return non-zero to
 * stop traversal. */
typedef int (*mptraverser)(struct mntent *entry, void *arg);

/* Checks whether the full_paths points to a location that is slow to access.
 * Returns non-zero if so, otherwise zero is returned. */
int is_on_slow_fs(const char full_path[], const char slowfs_specs[]);

/* Checks whether accessing the to location from the from location might cause
 * slowdown.  Returns non-zero if so, otherwise zero is returned. */
int refers_to_slower_fs(const char from[], const char to[]);

/* Fills supplied buffer with user friendly representation of file size.
 * Returns non-zero in case resulting string is a shortened variant of size. */
int friendly_size_notation(uint64_t num, int str_size, char str[]);

/* Returns pointer to a statically allocated buffer. */
const char * enclose_in_dquotes(const char str[], ShellType shell_type);

/* Changes current working directory of the process.  Does nothing if we already
 * at path.  Returns zero on success, otherwise -1 is returned. */
int vifm_chdir(const char path[]);

/* Expands all environment variables and tilde in the path.  Allocates
 * memory, that should be freed by the caller. */
char * expand_path(const char path[]);

/* Expands all environment variables in the str of the form "$envvar".  Flags is
 * a combination of EEF_* values.  Allocates and returns memory that should be
 * freed by the caller. */
char * expand_envvars(const char str[], int flags);

/* Makes filename unique by adding an unique suffix to it.
 * Returns pointer to a statically allocated buffer */
const char * make_name_unique(const char filename[]);

/* Returns process identification in a portable way. */
unsigned int get_pid(void);

/* Finds command name in the command line and writes it to the buf.
 * Raw mode will preserve quotes on Windows.
 * Returns a pointer to the argument list. */
char * extract_cmd_name(const char line[], int raw, size_t buf_len, char buf[]);

/* Determines columns needed for displaying a wide character.  Extends standard
 * wcwidth() with support of non-printable characters.  Returns number of
 * columns, on error default value of 1 is returned. */
int vifm_wcwidth(wchar_t c);

/* Determines columns needed for displaying a string of wide characters of
 * length at most n.  Extends standard wcswidth() with support of non-printable
 * characters.  Returns number of columns, on error default value of 1 is
 * returned. */
int vifm_wcswidth(const wchar_t str[], size_t n);

/* Escapes string from offset position until its end for insertion into single
 * quoted string, prefix is not escaped.  Returns newly allocated string. */
char * escape_for_squotes(const char string[], size_t offset);

/* Escapes string from offset position until its end for insertion into double
 * quoted string, prefix is not escaped.  Returns newly allocated string. */
char * escape_for_dquotes(const char string[], size_t offset);

/* Escapes characters that aren't visible or don't look nice to the user.
 * Returns newly allocated string. */
char * escape_unreadable(const char str[]);

/* Calculates escaping overhead for some prefix of the string.  Returns
 * byte length difference between unchanged prefix and prefix after escaping,
 * which might be positive, zero or negative. */
int escape_unreadableo(const char str[], int prefix_len);

/* Wide version of escape_unreadable(). */
wchar_t * escape_unreadablew(const wchar_t str[]);

/* Expands double percent sequences into single percent character in place. */
void expand_percent_escaping(char s[]);

/* Expands double ' sequences from single quoted string in place. */
void expand_squotes_escaping(char s[]);

/* Expands escape sequences from double quoted string (e.g. "\n") in place. */
void expand_dquotes_escaping(char s[]);

/* Gets correct register to operate on (user choice or the default one).
 * Returns register name. */
int def_reg(int reg);

/* Gets correct count (user choice or the default one (1)).  Returns the
 * count. */
int def_count(int count);

/* Wrapper around qsort() that allows base to be NULL when nmemb is 0 by
 * skipping call to qsort(). */
void safe_qsort(void *base, size_t nmemb, size_t size,
		int (*compar)(const void *, const void *));

/* Formats position within the viewport as one of the following: All, Top, xx%
 * or Bot. */
void format_position(char buf[], size_t buf_len, int top, int total,
		int visible);

/* Makes input file for a command if requested.  Returns the file or NULL if
 * it's not necessary. */
FILE * make_in_file(struct view_t *view, MacroFlags flags);

/* Writes list of marked files to the file.  Files are separated by new line or
 * null characters. */
void write_marked_paths(FILE *file, struct view_t *view, int null_sep);

/* Formats time as a string independent of settings.  Writes empty string on
 * error. */
void format_iso_time(time_t t, char buf[], size_t buf_size);

/* Checks line for path in it.  Ignores empty lines and attempts to parse it as
 * location line (path followed by a colon and optional line and column
 * numbers).  Returns canonicalized path as a newly allocated string or NULL. */
char * parse_line_for_path(const char line[], const char cwd[]);

/* Extracts path and line number from the spec (default line number is 1).
 * Returns path in as newly allocated string and sets *line_num to line number,
 * otherwise NULL is returned. */
char * parse_file_spec(const char spec[], int *line_num, const char cwd[]);

/* Fills buf of the length buf_len with path to mount point of the path.
 * Returns non-zero on error, otherwise zero is returned. */
int get_mount_point(const char path[], size_t buf_len, char buf[]);

/* Calls client traverser for each mount point.  Returns non-zero on error,
 * otherwise zero is returned. */
int traverse_mount_points(mptraverser client, void *arg);

struct cancellation_t;

/* Waits until non-blocking read operation is available for given file
 * descriptor (uses f if it's not NULL, otherwise fd is used) that is associated
 * with a process.  Process operation cancellation requests from a user. */
void wait_for_data_from(pid_t pid, FILE *f, int fd,
		const struct cancellation_t *cancellation);

/* Blocks all signals of current thread that can be blocked. */
void block_all_thread_signals(void);

/* Checks for executable by its path.  Mutates path by appending executable
 * prefixes on Windows.  Returns non-zero if path points to an executable,
 * otherwise zero is returned. */
int executable_exists(const char path[]);

/* Fills dir_buf of length dir_buf_len with full path to the directory where
 * executable of the application is located.  Returns zero on success, otherwise
 * non-zero is returned. */
int get_exe_dir(char dir_buf[], size_t dir_buf_len);

/* Gets type of operating environment the application is running in.  Returns
 * the type. */
EnvType get_env_type(void);

/* Determines type of active execution environment.  Returns the type. */
ExecEnvType get_exec_env_type(void);

/* Determines kind of the shell by its invocation command.  Returns the kind. */
ShellType get_shell_type(const char shell_cmd[]);

/* Formats command to view documentation in plain-text format.  Returns non-zero
 * if command that should be run in background, otherwise zero is returned. */
int format_help_cmd(char cmd[], size_t cmd_size);

/* Displays documentation in plain-text format without detaching from the
 * terminal. */
void display_help(const char cmd[]);

/* Suspends process until external signal comes. */
void wait_for_signal(void);

/* Suspends process as a terminal job. */
void stop_process(void);

/* Resets terminal to its normal state, if needed.  E.g. after some programs run
 * by Vifm messed it up. */
void update_terminal_settings(void);

/* Fills the buffer with string representation of owner user for the entry.  The
 * as_num flag forces formatting as integer. */
void get_uid_string(const struct dir_entry_t *entry, int as_num, size_t buf_len,
		char buf[]);

/* Fills the buffer with string representation of owner group for the entry.
 * The as_num flag forces formatting as integer. */
void get_gid_string(const struct dir_entry_t *entry, int as_num, size_t buf_len,
		char buf[]);

/* Reopens real terminal and binds it to stdout.  Returns NULL on error (message
 * is printed to stderr), otherwise file previously used as stdout is
 * returned. */
FILE * reopen_term_stdout(void);

/* Reopens real terminal and binds it to stdin.  Returns non-zero on error
 * (message is printed to stderr) and zero otherwise. */
int reopen_term_stdin(void);

/* Executes the command via shell and opens its output for reading.  Returns
 * NULL on error, otherwise stream valid for reading is returned. */
FILE * read_cmd_output(const char cmd[], int preserve_stdin);

/* Gets path to directory where files bundled with Vifm are stored.  Returns
 * pointer to a statically allocated buffer. */
const char * get_installed_data_dir(void);

/* Gets path to directory where global configuration files of Vifm are stored.
 * Returns pointer to a statically allocated buffer. */
const char * get_sys_conf_dir(void);

/* Clones attributes from file specified by from to file at path.  st is a hint
 * to omit extra file system requests if possible, can be NULL on Windows. */
void clone_attribs(const char path[], const char from[], const struct stat *st);

/* Retrieves drive information for location specified by the path.  Returns zero
 * on success and non-zero otherwise. */
int get_drive_info(const char at[], uint64_t *total_bytes,
		uint64_t *free_bytes);

/* Retrieves inode number that corresponds to the entry by resolving symbolic
 * links if necessary.  Returns the inode number. */
uint64_t get_true_inode(const struct dir_entry_t *entry);

/* Auxiliary function for binary search in interval table.  Returns non-zero
 * if search was successful and zero otherwise. */
int unichar_bisearch(wchar_t ucs, const interval_t table[], int max);

/* Checks whether Unicode character is printable.  Returns non-zero if so,
 * otherwise zero is returned. */
int unichar_isprint(wchar_t ucs);

#ifdef _WIN32
#include "utils_win.h"
#else
#include "utils_nix.h"
#endif

#endif /* VIFM__UTILS__UTILS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
