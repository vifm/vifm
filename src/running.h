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

/* When and why should we pause on shellout(). */
typedef enum
{
	PAUSE_ALWAYS,   /* Execute command and pause. */
	PAUSE_NEVER,    /* Don't pause after the command. */
	PAUSE_ON_ERROR, /* Pause only on non-zero exit code from the command. */
}
ShellPause;

struct view_t;

/* Handles opening of current file/selection of the view. */
void open_file(struct view_t *view, FileHandleExec exec);

/* Follows file to find its true location (e.g. target of symbolic link) or just
 * opens it. */
void follow_file(struct view_t *view);

/* Runs current file of the view guided by program specification with additional
 * options. */
void run_using_prog(struct view_t *view, const char prog_spec[],
		int dont_execute, int force_bg);

/* Handles opening of current file of the view as directory. */
void open_dir(struct view_t *view);

/* Moves the view to levels-th parent directory taking care of special cases
 * like root of FUSE mount. */
void cd_updir(struct view_t *view, int levels);

/* Executes command in a shell.  Returns zero on success, otherwise non-zero is
 * returned. */
int shellout(const char command[], ShellPause pause, int use_term_multiplexer,
		ShellRequester by);

/* Returns zero on successful running. */
int run_with_filetype(struct view_t *view, const char beginning[],
		int background);

/* Handles most of command handling variants.  Returns:
 *  - > 0 -- handled, good to go;
 *  - = 0 -- not handled at all;
 *  - < 0 -- handled, exit. */
int run_ext_command(const char cmd[], MacroFlags flags, int bg, int *save_msg);

/* Runs the cmd and parses its output as list of paths to compose custom view.
 * Very custom view implies unsorted list.  Returns zero on success, otherwise
 * non-zero is returned. */
int output_to_custom_flist(struct view_t *view, const char cmd[], int very,
		int interactive);

/* Executes external command capturing its output as list of lines.  Sets *files
 * and *nfiles.  Returns zero on success, otherwise non-zero is returned. */
int run_cmd_for_output(const char cmd[], char ***files, int *nfiles);

#endif /* VIFM__RUNNING_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
