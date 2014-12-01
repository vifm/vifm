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

#include "vim.h"

#include <unistd.h> /* F_OK access() */

#include <errno.h> /* errno */
#include <stdio.h> /* FILE fclose() fopen() fprintf() snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strrchr() strstr() */

#include "cfg/config.h"
#include "cfg/info.h"
#include "menus/menus.h"
#include "utils/fs.h"
#include "utils/log.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/test_helpers.h"
#include "utils/utils.h"
#include "background.h"
#include "macros.h"
#include "running.h"
#include "ui.h"

#define LIST_FILE "vimfiles"

TSTATIC char * format_edit_selection_cmd(int *bg);
static void dump_filenames(const FileView *view, FILE *fp, int nfiles,
		char *files[]);

int
vim_format_help_cmd(const char topic[], char cmd[], size_t cmd_size)
{
	int bg;

#ifndef _WIN32
	char *const escaped_rtp = escape_filename(PACKAGE_DATA_DIR, 0);
	char *const escaped_args = escape_filename(topic, 0);

	snprintf(cmd, cmd_size,
			"%s -c 'set runtimepath+=%s/vim-doc' -c help\\ %s -c only",
			get_vicmd(&bg), escaped_rtp, escaped_args);

	free(escaped_args);
	free(escaped_rtp);
#else
	char exe_dir[PATH_MAX];
	char *escaped_rtp;

	(void)get_exe_dir(exe_dir, sizeof(exe_dir));
	escaped_rtp = escape_filename(exe_dir, 0);

	snprintf(cmd, cmd_size,
			"%s -c \"set runtimepath+=%s/data/vim-doc\" -c \"help %s\" -c only",
			get_vicmd(&bg), escaped_rtp, topic);

	free(escaped_rtp);
#endif

	return bg;
}

void
vim_edit_files(int nfiles, char *files[])
{
	char buf[PATH_MAX];
	size_t len;
	int i;
	int bg;

	len = snprintf(buf, sizeof(buf), "%s ", get_vicmd(&bg));
	for(i = 0; i < nfiles && len < sizeof(buf) - 1; i++)
	{
		char *escaped = escape_filename(files[i], 0);
		len += snprintf(buf + len, sizeof(buf) - len, "%s ", escaped);
		free(escaped);
	}

	if(bg)
		start_background_job(buf, 0);
	else
		shellout(buf, -1, 1);
}

int
vim_edit_selection(void)
{
	int error = 1;
	int bg;
	char *const cmd = format_edit_selection_cmd(&bg);
	if(cmd != NULL)
	{
		/* TODO: move next line to a separate function. */
		error = bg ? start_background_job(cmd, 0) : shellout(cmd, -1, 1);
		free(cmd);
	}
	return error;
}

/* Formats a command to edit selected files of the current view in an editor.
 * Returns a newly allocated string, which should be freed by the caller. */
TSTATIC char *
format_edit_selection_cmd(int *bg)
{
	char *const files = expand_macros("%f", NULL, NULL, 1);
	char *const cmd = format_str("%s %s", get_vicmd(bg), files);
	free(files);
	return cmd;
}

int
vim_view_file(const char filename[], int line, int column, int allow_forking)
{
	char vicmd[PATH_MAX];
	char command[PATH_MAX + 5] = "";
	const char *fork_str = allow_forking ? "" : "--nofork";
	char *escaped;
	int bg;
	int result;

	if(!path_exists(filename))
	{
		if(access(filename, F_OK) != 0)
		{
			show_error_msg("Broken Link", "Link destination doesn't exist");
		}
		else
		{
			show_error_msg("Wrong Path", "File doesn't exist");
		}
		return 1;
	}

#ifndef _WIN32
	escaped = escape_filename(filename, 0);
#else
	escaped = (char *)enclose_in_dquotes(filename);
#endif

	snprintf(vicmd, sizeof(vicmd), "%s", get_vicmd(&bg));
	(void)trim_right(vicmd);
	if(!allow_forking)
	{
		char *p = strrchr(vicmd, ' ');
		if(p != NULL && strstr(p, "remote"))
		{
			*p = '\0';
		}
	}

	if(line < 0 && column < 0)
		snprintf(command, sizeof(command), "%s %s %s", vicmd, fork_str, escaped);
	else if(column < 0)
		snprintf(command, sizeof(command), "%s %s +%d %s", vicmd, fork_str, line,
				escaped);
	else
		snprintf(command, sizeof(command), "%s %s \"+call cursor(%d, %d)\" %s",
				vicmd, fork_str, line, column, escaped);

#ifndef _WIN32
	free(escaped);
#endif

	if(bg && allow_forking)
	{
		result = start_background_job(command, 0);
	}
	else
	{
		result = shellout(command, -1, allow_forking);
	}
	curs_set(FALSE);

	return result;
}

void _gnuc_noreturn
vim_return_file_list(const FileView *view, int nfiles, char *files[])
{
	FILE *fp;
	char filepath[PATH_MAX];
	int exit_code = EXIT_SUCCESS;

	snprintf(filepath, sizeof(filepath), "%s/" LIST_FILE, cfg.config_dir);
	fp = fopen(filepath, "w");
	if(fp != NULL)
	{
		dump_filenames(view, fp, nfiles, files);
		fclose(fp);
	}
	else
	{
		LOG_SERROR_MSG(errno, "Can't open file for writing: \"%s\"", filepath);
		exit_code = EXIT_FAILURE;
	}

	write_info_file();

	endwin();
	exit(exit_code);
}

/* Writes list of full paths to files into the file pointed to by fp.  files and
 * nfiles parameters can be used to supply list of file names in the currecnt
 * directory of the view.  Otherwise current selection is used if current files
 * is selected, if current file is not selected it's the only one that is
 * stored. */
static void
dump_filenames(const FileView *view, FILE *fp, int nfiles, char *files[])
{
	if(nfiles == 0)
	{
		const dir_entry_t *const entry = &view->dir_entry[view->list_pos];
		if(!entry->selected)
		{
			fprintf(fp, "%s/%s\n", entry->origin, entry->name);
		}
		else
		{
			int i;
			for(i = 0; i < view->list_rows; ++i)
			{
				const dir_entry_t *const entry = &view->dir_entry[i];
				if(entry->selected)
				{
					fprintf(fp, "%s/%s\n", entry->origin, entry->name);
				}
			}
		}
	}
	else
	{
		int i;
		for(i = 0; i < nfiles; ++i)
		{
			if(is_path_absolute(files[i]))
			{
				fprintf(fp, "%s\n", files[i]);
			}
			else
			{
				fprintf(fp, "%s/%s\n", view->curr_dir, files[i]);
			}
		}
	}
}

void
vim_write_empty_file_list(void)
{
	char buf[PATH_MAX];
	FILE *fp;

	snprintf(buf, sizeof(buf), "%s/" LIST_FILE, cfg.config_dir);
	fp = fopen(buf, "w");
	if(fp != NULL)
	{
		fclose(fp);
	}
	else
	{
		LOG_SERROR_MSG(errno, "Can't truncate file: \"%s\"", buf);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
