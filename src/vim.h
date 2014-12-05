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

#ifndef VIFM__VIM_H__
#define VIFM__VIM_H__

#include <stddef.h> /* size_t */

#include "ui/ui.h"
#include "utils/macros.h"
#include "utils/test_helpers.h"

/* Formats command to display documentation on the topic in Vim-help format.
 * Returns non-zero if command that should be run in background, otherwise zero
 * is returned. */
int vim_format_help_cmd(const char topic[], char cmd[], size_t cmd_size);

/* Opens external editor to edit files specified by their names in passed
 * list. */
void vim_edit_files(int nfiles, char *files[]);

/* Opens external editor to edit selected files of the current view.  Returns
 * non-zero on error, otherwise zero is returned. */
int vim_edit_selection(void);

/* Negative line/column means ignore parameter.  First line/column number has
 * number one, while zero means don't change it.  Returns zero on success, on
 * error non-zero is returned. */
int vim_view_file(const char filename[], int line, int column,
		int allow_forking);

/* Leaves vifm storing list of file names (taken from the view or the files) in
 * a special file for use by an external application. */
void _gnuc_noreturn vim_return_file_list(const FileView *view, int nfiles,
		char *files[]);

/* Writes empty output file list meaning that user choice is empty. */
void vim_write_empty_file_list(void);

TSTATIC_DEFS(
	char * format_edit_selection_cmd(int *bg);
)

#endif /* VIFM__VIM_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
