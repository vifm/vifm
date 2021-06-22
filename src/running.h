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

#ifndef VIFM__RUNNING_H__
#define VIFM__RUNNING_H__

#include "utils/utils.h"
#include "macros.h"

/* Kinds of executable file treatment on file handling. */
typedef enum
{
	FHE_NO_RUN,          /* Don't run. */
	FHE_RUN,             /* Run with rights of current user. */
	FHE_ELEVATE_AND_RUN, /* Run with rights elevation (if available). */
}
FileHandleExec;

/* When and why should we pause on rn_shell(). */
typedef enum
{
	PAUSE_ALWAYS,   /* Execute command and pause. */
	PAUSE_NEVER,    /* Don't pause after the command. */
	PAUSE_ON_ERROR, /* Pause only on non-zero exit code from the command. */
}
ShellPause;

struct view_t;

/* Handles opening of current file/selection of the view. */
void rn_open(struct view_t *view, FileHandleExec exec);

/* Follows file to find its true location (e.g. target of symbolic link) or just
 * opens it. */
void rn_follow(struct view_t *view, int ultimate);

/* Runs current file of the view guided by program specification with additional
 * options. */
void rn_open_with(struct view_t *view, const char prog_spec[], int dont_execute,
		int force_bg);

/* Moves the view to levels-th parent directory taking care of special cases
 * like root of FUSE mount. */
void rn_leave(struct view_t *view, int levels);

/* Executes command in a shell.  Passing NULL for command parameter is a special
 * case of launching a shell.  Returns zero on success, otherwise non-zero is
 * returned. */
int rn_shell(const char command[], ShellPause pause, int use_term_multiplexer,
		ShellRequester by);

/* Same as rn_shell(), but provides the command with custom input. */
int rn_pipe(const char command[], struct view_t *view, MacroFlags flags,
		ShellPause pause);

/* Looks for a unique program match for a given prefix and uses it.  Returns
 * zero on success and non-zero otherwise.  */
int rn_open_with_match(struct view_t *view, const char beginning[],
		int background);

/* Handles most of command handling variants.  Returns:
 *  - > 0 -- handled, good to go;
 *  - = 0 -- not handled at all;
 *  - < 0 -- handled, exit. */
int rn_ext(struct view_t *view, const char cmd[], const char title[],
		MacroFlags flags, int bg, int *save_msg);

/* Starts background command optionally handling input redirection. */
void rn_start_bg_command(struct view_t *view, const char cmd[],
		MacroFlags flags);

/* Runs the cmd and parses its output as list of paths to compose custom view
 * or very custom view.  Returns zero on success, otherwise non-zero is
 * returned. */
int rn_for_flist(struct view_t *view, const char cmd[], const char title[],
		MacroFlags flags);

/* Executes external command capturing its output as list of lines.  Sets *lines
 * and *nlines.  Returns zero on success, otherwise non-zero is returned. */
int rn_for_lines(struct view_t *view, const char cmd[], char ***lines,
		int *nlines, MacroFlags flags);

#endif /* VIFM__RUNNING_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
