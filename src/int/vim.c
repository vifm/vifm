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

#include <curses.h> /* FALSE curs_set() */

#include <ctype.h> /* isspace() */
#include <errno.h> /* errno */
#include <stdio.h> /* FILE fclose() fprintf() fputs() snprintf() */
#include <stdlib.h> /* EXIT_SUCCESS free() */
#include <string.h> /* strcmp() strlen() strrchr() strstr() */

#include "../cfg/config.h"
#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../lua/vlua.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../ui/ui.h"
#include "../utils/fs.h"
#include "../utils/log.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/test_helpers.h"
#include "../background.h"
#include "../filelist.h"
#include "../flist_sel.h"
#include "../running.h"
#include "../status.h"
#include "../vifm.h"

/* File name known to Vim-plugin. */
#define LIST_FILE "vimfiles"

static int run_vim(const char cmd[], int bg, int use_term_multiplexer);
TSTATIC void trim_right(char text[]);
static void dump_filenames(view_t *view, FILE *fp, int nfiles, char *files[]);

int
vim_format_help_cmd(const char topic[], char cmd[], size_t cmd_size)
{
	int bg;

#ifndef _WIN32
	char *const escaped_rtp = shell_like_escape(PACKAGE_DATA_DIR, 0);
	char *const escaped_args = shell_like_escape(topic, 0);

	snprintf(cmd, cmd_size,
			"%s -c 'set runtimepath+=%s/vim-doc' -c help\\ %s -c only",
			cfg_get_vicmd(&bg), escaped_rtp, escaped_args);

	free(escaped_args);
	free(escaped_rtp);
#else
	char exe_dir[PATH_MAX + 1];
	char *escaped_rtp;

	(void)get_exe_dir(exe_dir, sizeof(exe_dir));
	escaped_rtp = shell_like_escape(exe_dir, 0);

	snprintf(cmd, cmd_size,
			"%s -c \"set runtimepath+=%s/data/vim-doc\" -c \"help %s\" -c only",
			cfg_get_vicmd(&bg), escaped_rtp, topic);

	free(escaped_rtp);
#endif

	return bg;
}

int
vim_edit_files(int nfiles, char *files[])
{
	int i;
	int bg;
	int error;

	const char *vi_cmd = cfg_get_vicmd(&bg);

	if(vlua_handler_cmd(curr_stats.vlua, vi_cmd))
	{
		if(vlua_edit_many(curr_stats.vlua, vi_cmd, files, nfiles) != 0)
		{
			show_error_msg("File View", "Failed to view files via handler");
			return 1;
		}
		return 0;
	}

	char *cmd = NULL;
	size_t len = 0U;
	(void)strappend(&cmd, &len, vi_cmd);

	for(i = 0; i < nfiles; ++i)
	{
		char *const expanded_path = expand_tilde(files[i]);
		char *const escaped = shell_like_escape(expanded_path, 0);
		(void)strappendch(&cmd, &len, ' ');
		(void)strappend(&cmd, &len, escaped);
		free(escaped);
		free(expanded_path);
	}

	error = (cmd == NULL || run_vim(cmd, bg, 1));
	free(cmd);

	return error;
}

int
vim_edit_marking(view_t *view)
{
	const int cv = flist_custom_active(view);

	char **marked = NULL;
	int nmarked = 0;

	dir_entry_t *entry = NULL;
	while(iter_marked_entries(view, &entry))
	{
		if(!cv)
		{
			nmarked = add_to_string_array(&marked, nmarked, entry->name);
			continue;
		}

		char path[PATH_MAX + 1];
		get_short_path_of(view, entry, NF_NONE, 0, sizeof(path), path);
		nmarked = add_to_string_array(&marked, nmarked, path);
	}

	int result = vim_edit_files(nmarked, marked);

	free_string_array(marked, nmarked);
	return result;
}

int
vim_view_file(const char filename[], int line, int column, int allow_forking)
{
	char vicmd[PATH_MAX + 1];
	char cmd[2*PATH_MAX + 5];
	/* Use `-f` and not `--nofork` here to make it for with nvim (and possibly
	 * other flavours of vi). */
	const char *fork_str = allow_forking ? "" : "-f";
	char *escaped;
	int bg;
	int result;

	cmd[0] = '\0';

	if(!path_exists(filename, DEREF))
	{
		if(path_exists(filename, NODEREF))
		{
			show_error_msg("Broken Link", "Link destination doesn't exist");
		}
		else
		{
			show_error_msg("Wrong Path", "File doesn't exist");
		}
		return 1;
	}

	copy_str(vicmd, sizeof(vicmd), cfg_get_vicmd(&bg));

	if(vlua_handler_cmd(curr_stats.vlua, vicmd))
	{
		if(vlua_edit_one(curr_stats.vlua, vicmd, filename, line, column,
					!allow_forking) != 0)
		{
			show_error_msg("File View", "Failed to view file via handler");
			return 1;
		}
		return 0;
	}

	trim_right(vicmd);
	if(!allow_forking)
	{
		char *p = strrchr(vicmd, ' ');
		if(p != NULL && strstr(p, "remote"))
		{
			*p = '\0';
		}
	}

#ifndef _WIN32
	escaped = shell_like_escape(filename, 0);
#else
	escaped = (char *)enclose_in_dquotes(filename, curr_stats.shell_type);
#endif

	if(line < 0 && column < 0)
		snprintf(cmd, sizeof(cmd), "%s %s %s", vicmd, fork_str, escaped);
	else if(column < 0)
		snprintf(cmd, sizeof(cmd), "%s %s +%d %s", vicmd, fork_str, line, escaped);
	else
		snprintf(cmd, sizeof(cmd), "%s %s \"+call cursor(%d, %d)\" %s", vicmd,
				fork_str, line, column, escaped);

#ifndef _WIN32
	free(escaped);
#endif

	result = run_vim(cmd, bg && allow_forking, allow_forking);

	/* The check is for tests. */
	if(curr_stats.load_stage > 0)
	{
		curs_set(0);
	}

	return result;
}

/* Removes all trailing whitespace. */
TSTATIC void
trim_right(char str[])
{
	size_t len = strlen(str);
	while(len > 0U && isspace(str[len - 1U]))
	{
		str[--len] = '\0';
	}
}

/* Runs command with specified settings.  Returns exit code of the command. */
static int
run_vim(const char cmd[], int bg, int use_term_multiplexer)
{
	if(bg)
	{
		return bg_run_external(cmd, 0, SHELL_BY_APP, NULL);
	}

	return rn_shell(cmd, PAUSE_ON_ERROR, use_term_multiplexer, SHELL_BY_APP);
}

int
vim_write_file_list(view_t *view, int nfiles, char *files[])
{
	FILE *fp;
	const char *const files_out = curr_stats.chosen_files_out;

	if(is_null_or_empty(files_out))
	{
		return 0;
	}

	if(strcmp(files_out, "-") == 0)
	{
		dump_filenames(view, curr_stats.original_stdout, nfiles, files);
		return 0;
	}

	fp = os_fopen(files_out, "w");
	if(fp == NULL)
	{
		LOG_SERROR_MSG(errno, "Can't open file for writing: \"%s\"", files_out);
		return 1;
	}

	dump_filenames(view, fp, nfiles, files);
	fclose(fp);
	return 0;
}

/* Writes list of full paths to files into the file pointed to by fp.  files and
 * nfiles parameters can be used to supply list of file names in the current
 * directory of the view.  Otherwise current selection is used if current files
 * is selected, if current file is not selected it's the only one that is
 * stored. */
static void
dump_filenames(view_t *view, FILE *fp, int nfiles, char *files[])
{
	/* Break delimiter in it's first character and the rest to be able to insert
	 * null character via "%c%s" format string. */
	const char delim_c = curr_stats.output_delimiter[0];
	const char *const delim_str = curr_stats.output_delimiter[0] == '\0'
	                            ? ""
	                            : curr_stats.output_delimiter + 1;

	if(nfiles == 0)
	{
		dir_entry_t *entry = NULL;
		while(iter_marked_entries(view, &entry))
		{
			const char *const sep = (ends_with_slash(entry->origin) ? "" : "/");
			fprintf(fp, "%s%s%s%c%s", entry->origin, sep, entry->name, delim_c,
					delim_str);
		}
	}
	else
	{
		int i;
		for(i = 0; i < nfiles; ++i)
		{
			if(is_path_absolute(files[i]))
			{
				fprintf(fp, "%s%c%s", files[i], delim_c, delim_str);
			}
			else
			{
				const char *const dir = flist_get_dir(view);
				const char *const sep = (ends_with_slash(dir) ? "" : "/");
				fprintf(fp, "%s%s%s%c%s", dir, sep, files[i], delim_c, delim_str);
			}
		}
	}
}

void
vim_write_empty_file_list(void)
{
	FILE *fp;
	const char *const files_out = curr_stats.chosen_files_out;

	if(is_null_or_empty(files_out) || strcmp(files_out, "-") == 0)
	{
		return;
	}

	fp = os_fopen(files_out, "w");
	if(fp != NULL)
	{
		fclose(fp);
	}
	else
	{
		LOG_SERROR_MSG(errno, "Can't truncate file: \"%s\"", files_out);
	}
}

void
vim_write_dir(const char path[])
{
	/* TODO: move this and other non-Vim related code to extern.c unit. */

	FILE *fp;
	const char *const dir_out = curr_stats.chosen_dir_out;

	if(is_null_or_empty(dir_out))
	{
		return;
	}

	if(strcmp(dir_out, "-") == 0)
	{
		fputs(path, curr_stats.original_stdout);
		putc('\n', curr_stats.original_stdout);
		return;
	}

	fp = os_fopen(dir_out, "w");
	if(fp == NULL)
	{
		LOG_SERROR_MSG(errno, "Can't open file for writing: \"%s\"", dir_out);
		return;
	}

	fputs(path, fp);
	putc('\n', fp);
	fclose(fp);
}

int
vim_run_choose_cmd(view_t *view)
{
	char *expanded_cmd;

	if(is_null_or_empty(curr_stats.on_choose))
	{
		return 0;
	}

	expanded_cmd = ma_expand(curr_stats.on_choose, NULL, NULL, MER_SHELL_OP);
	if(vifm_system(expanded_cmd, SHELL_BY_USER) != EXIT_SUCCESS)
	{
		free(expanded_cmd);
		return 1;
	}

	return 0;
}

void
vim_get_list_file_path(char buf[], size_t buf_size)
{
	snprintf(buf, buf_size, "%s/" LIST_FILE, cfg.config_dir);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
