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

#include "utils/macros.h"
#include "utils/test_helpers.h"
#include "ui.h"

void handle_file(FileView *view, int dont_execute, int force_follow);
/* Opens external editor to edit selected files of the current view.  Returns
 * non-zero on error, otherwise zero is returned. */
int edit_selection(void);
void run_using_prog(FileView *view, const char *program, int dont_execute,
		int force_background);
/* Negative line/column means ignore parameter.  First line/column number has
 * number one, while zero means don't change it.  Returns zero on success, on
 * error non-zero is returned. */
int view_file(const char filename[], int line, int column, int do_fork);
void handle_dir(FileView *view);
void cd_updir(FileView *view);
void _gnuc_noreturn use_vim_plugin(FileView *view, int argc, char **argv);
/* Returns zero on success, otherwise non-zero is returned. */
int shellout(const char *command, int pause, int allow_screen);
void output_to_nowhere(const char *cmd);
/* Returns zero on successful running. */
int run_with_filetype(FileView *view, const char beginning[], int background);

TSTATIC_DEFS(
	char * format_edit_selection_cmd(int *bg);
)

#endif /* VIFM__RUNNING_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
