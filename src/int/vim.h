/* vifm
 * Copyright (C) 2014 xaizek.
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

#ifndef VIFM__INT__VIM_H__
#define VIFM__INT__VIM_H__

#include <stddef.h> /* size_t */

#include "../utils/test_helpers.h"

struct view_t;

/* Formats command to display documentation on the topic in Vim-help format.
 * Returns non-zero if command that should be run in background, otherwise zero
 * is returned. */
int vim_format_help_cmd(const char topic[], char cmd[], size_t cmd_size);

/* Opens external editor to edit files specified by their names in passed
 * list.  Returns zero on success, otherwise non-zero is returned. */
int vim_edit_files(int nfiles, char *files[]);

/* Opens external editor to edit marked files of the view.  Returns non-zero on
 * error, otherwise zero is returned. */
int vim_edit_marking(struct view_t *view);

/* Negative line/column means ignore parameter.  First line/column number has
 * number one, while zero means don't change it.  Returns zero on success, on
 * error non-zero is returned. */
int vim_view_file(const char filename[], int line, int column,
		int allow_forking);

/* Stores list of file names (active files taken from the view or the files) in
 * a special file for use by an external application.  Returns zero on success,
 * otherwise non-zero is returned. */
int vim_write_file_list(struct view_t *view, int nfiles, char *files[]);

/* Writes empty output file list meaning that user choice is empty. */
void vim_write_empty_file_list(void);

/* Writes the path to last visited path output configured by user on
 * command-line. */
void vim_write_dir(const char path[]);

/* Runs user-specified command on selection.  Returns zero on success, otherwise
 * non-zero is returned. */
int vim_run_choose_cmd(struct view_t *view);

/* Fills the buffer of length buf_size with path to default file list location
 * for the plugin. */
void vim_get_list_file_path(char buf[], size_t buf_size);

TSTATIC_DEFS(
	void trim_right(char str[]);
)

#endif /* VIFM__INT__VIM_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
