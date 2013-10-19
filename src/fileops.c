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

#include "fileops.h"

#include <regex.h>

#include <curses.h>

#include <pthread.h>

#include <dirent.h> /* DIR */
#include <fcntl.h>
#include <sys/stat.h> /* stat */
#include <sys/types.h> /* waitpid() */
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif
#include <unistd.h>

#include <assert.h> /* assert() */
#include <ctype.h> /* isdigit() */
#include <errno.h> /* errno */
#include <signal.h>
#include <stddef.h> /* size_t */
#include <stdint.h> /* uint64_t */
#include <stdio.h>
#include <string.h> /* memcmp() strcpy() strerror() */

#include "cfg/config.h"
#include "menus/menus.h"
#include "modes/cmdline.h"
#ifdef _WIN32
#include "utils/env.h"
#endif
#include "utils/fs.h"
#include "utils/fs_limits.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/test_helpers.h"
#include "utils/utils.h"
#include "background.h"
#include "color_scheme.h"
#include "commands.h"
#include "commands_completion.h"
#include "filelist.h"
#include "filetype.h"
#include "ops.h"
#include "registers.h"
#include "running.h"
#include "status.h"
#include "ui.h"
#include "undo.h"

static char rename_file_ext[NAME_MAX];

static struct
{
	registers_t *reg;
	FileView *view;
	int force_move;
	int x, y;
	char *name;
	int overwrite_all;
	int allow_merge;
	int merge;
	int link; /* 0 - no, 1 - absolute, 2 - relative */
}
put_confirm;

typedef struct
{
	char **list;
	int nlines;
	int move;
	int force;
	char **sel_list;
	size_t sel_list_len;
	char src[PATH_MAX];
	char path[PATH_MAX];
	int from_file;
	int from_trash;
	job_t *job;
}bg_args_t;

static int prepare_register(int reg);
static void delete_file_bg_i(const char curr_dir[], char *list[], int count,
		int use_trash);
TSTATIC int is_name_list_ok(int count, int nlines, char *list[], char *files[]);
TSTATIC int is_rename_list_ok(char *files[], int *is_dup, int len,
		char *list[]);
TSTATIC const char * add_to_name(const char filename[], int k);
TSTATIC int check_file_rename(const char dir[], const char old[],
		const char new[], SignalType signal_type);
static int is_file_name_changed(const char old[], const char new[]);
static void put_confirm_cb(const char *dest_name);
static void prompt_what_to_do(const char src_name[]);
TSTATIC const char * gen_clone_name(const char normal_name[]);
static void put_decide_cb(const char *dest_name);
static int entry_is_dir(const char full_path[], const struct dirent* dentry);
static int put_files_from_register_i(FileView *view, int start);
static int have_read_access(FileView *view);
static char ** edit_list(size_t count, char **orig, int *nlines,
		int ignore_change);
static int edit_file(const char filepath[], int force_changed);

/* returns new value for save_msg */
int
yank_files(FileView *view, int reg, int count, int *indexes)
{
	int yanked;
	if(count > 0)
		get_selected_files(view, count, indexes);
	else
		get_all_selected_files(view);

	yank_selected_files(view, reg);
	yanked = view->selected_files;
	free_selected_file_array(view);
	count_selected(view);

	if(count == 0)
	{
		clean_selected_files(view);
		redraw_view(view);
	}

	status_bar_messagef("%d %s yanked", yanked, yanked == 1 ? "file" : "files");

	return yanked;
}

void
yank_selected_files(FileView *view, int reg)
{
	int x;

	reg = prepare_register(reg);

	for(x = 0; x < view->selected_files; x++)
	{
		char buf[PATH_MAX];
		if(view->selected_filelist[x] == NULL)
			break;

		snprintf(buf, sizeof(buf), "%s%s%s", view->curr_dir,
				ends_with_slash(view->curr_dir) ? "" : "/", view->selected_filelist[x]);
		chosp(buf);
		append_to_register(reg, buf);
	}
	update_unnamed_reg(reg);
}

static void
progress_msg(const char *text, int ready, int total)
{
	char msg[strlen(text) + 32];

	sprintf(msg, "%s %d/%d", text, ready, total);
	show_progress(msg, 1);
	curr_stats.save_msg = 2;
}

/* returns string that needs to be released by caller */
static char *
gen_trash_name(const char *name)
{
	struct stat st;
	char buf[PATH_MAX];
	int i = 0;

	do
	{
		snprintf(buf, sizeof(buf), "%s/%03d_%s", cfg.trash_dir, i++, name);
		chosp(buf);
	}
	while(lstat(buf, &st) == 0);
	return strdup(buf);
}

/* buf should be at least COMMAND_GROUP_INFO_LEN characters length */
static void
get_group_file_list(char **list, int count, char *buf)
{
	size_t len;
	int i;

	len = strlen(buf);
	for(i = 0; i < count && len < COMMAND_GROUP_INFO_LEN; i++)
	{
		if(buf[len - 2] != ':')
		{
			strncat(buf, ", ", COMMAND_GROUP_INFO_LEN - len - 1);
			len = strlen(buf);
		}
		strncat(buf, list[i], COMMAND_GROUP_INFO_LEN - len - 1);
		len = strlen(buf);
	}
}

/* returns new value for save_msg */
int
delete_file(FileView *view, int reg, int count, int *indexes, int use_trash)
{
	char buf[MAX(COMMAND_GROUP_INFO_LEN, 8 + PATH_MAX*2)];
	int x, y;
	int i;

	if(!check_if_dir_writable(DR_CURRENT, view->curr_dir))
		return 0;

	if(cfg.use_trash && use_trash &&
			path_starts_with(view->curr_dir, cfg.trash_dir))
	{
		show_error_msg("Can't perform deletion",
				"Current directory is under Trash directory");
		return 0;
	}

	if(count > 0)
	{
		int j;
		get_selected_files(view, count, indexes);
		i = view->list_pos;
		for(j = 0; j < count; j++)
			if(indexes[j] == i && i < view->list_rows - 1)
			{
				i++;
				j = -1;
			}
	}
	else
	{
		get_all_selected_files(view);
		i = view->list_pos;
		while(i < view->list_rows - 1 && view->dir_entry[i].selected)
			i++;
	}
	view->list_pos = i;

	if(cfg.use_trash && use_trash)
	{
		reg = prepare_register(reg);
	}

	if(cfg.use_trash && use_trash)
		snprintf(buf, sizeof(buf), "delete in %s: ",
				replace_home_part(view->curr_dir));
	else
		snprintf(buf, sizeof(buf), "Delete in %s: ",
				replace_home_part(view->curr_dir));

	get_group_file_list(view->selected_filelist, view->selected_files, buf);
	cmd_group_begin(buf);

	y = 0;
	if(my_chdir(curr_view->curr_dir) != 0)
	{
		show_error_msg("Directory return", "Can't chdir() to current directory");
		return 1;
	}
	for(x = 0; x < view->selected_files; x++)
	{
		char full_buf[PATH_MAX];
		int result;

		if(is_parent_dir(view->selected_filelist[x]))
		{
			show_error_msg("Background Process Error",
					"You cannot delete the ../ directory");
			continue;
		}

		snprintf(full_buf, sizeof(full_buf), "%s/%s", view->curr_dir,
				view->selected_filelist[x]);
		chosp(full_buf);

		progress_msg("Deleting files", x + 1, view->selected_files);
		if(cfg.use_trash && use_trash)
		{
			if(stroscmp(full_buf, cfg.trash_dir) == 0)
			{
				show_error_msg("Background Process Error",
						"You cannot delete trash directory to trash");
				result = -1;
			}
			else
			{
				char *dest;

				dest = gen_trash_name(view->selected_filelist[x]);
				result = perform_operation(OP_MOVE, NULL, full_buf, dest);
				if(result == 0)
				{
					add_operation(OP_MOVE, NULL, NULL, full_buf, dest);
					append_to_register(reg, dest);
				}
				free(dest);
			}
		}
		else
		{
			result = perform_operation(OP_REMOVE, NULL, full_buf, NULL);
			if(result == 0)
				add_operation(OP_REMOVE, NULL, NULL, full_buf, "");
		}

		if(result == 0)
		{
			y++;
		}
		else if(!view->dir_entry[view->list_pos].selected)
		{
			int pos = find_file_pos_in_list(view, view->selected_filelist[x]);
			if(pos >= 0)
				view->list_pos = pos;
		}
	}
	free_selected_file_array(view);
	clean_selected_files(view);

	update_unnamed_reg(reg);

	cmd_group_end();

	load_saving_pos(view, 1);

	status_bar_messagef("%d %s deleted", y, y == 1 ? "file" : "files");
	return 1;
}

/* Transforms "A-"Z register to "a-"z or clears the reg.  So that for "A-"Z new
 * values will be appended to "a-"z, for other registers old values will be
 * removed.  Returns possibly modified value of the reg parameter. */
static int
prepare_register(int reg)
{
	if(reg >= 'A' && reg <= 'Z')
	{
		reg += 'a' - 'A';
	}
	else
	{
		clear_register(reg);
	}
	return reg;
}

static void *
delete_file_stub(void *arg)
{
	bg_args_t *args = (bg_args_t *)arg;

	add_inner_bg_job(args->job);

	delete_file_bg_i(args->src, args->sel_list, args->sel_list_len,
			args->from_trash);

	remove_inner_bg_job();

	free_string_array(args->sel_list, args->sel_list_len);
	free(args);
	return NULL;
}

static void
delete_file_bg_i(const char curr_dir[], char *list[], int count, int use_trash)
{
	int i;
	for(i = 0; i < count; i++)
	{
		char full_buf[PATH_MAX];

		if(is_parent_dir(list[i]))
			continue;

		snprintf(full_buf, sizeof(full_buf), "%s/%s", curr_dir, list[i]);
		chosp(full_buf);

		if(use_trash)
		{
			if(strcmp(full_buf, cfg.trash_dir) != 0)
			{
				char *dest;

				dest = gen_trash_name(list[i]);
				(void)perform_operation(OP_MOVE, NULL, full_buf, dest);
				free(dest);
			}
		}
		else
		{
			(void)perform_operation(OP_REMOVE, NULL, full_buf, NULL);
		}
		inner_bg_next();
	}
}

/* returns new value for save_msg */
int
delete_file_bg(FileView *view, int use_trash)
{
	pthread_t id;
	char buf[COMMAND_GROUP_INFO_LEN];
	int i;
	bg_args_t *args;
	
	if(!check_if_dir_writable(DR_CURRENT, view->curr_dir))
		return 0;

	args = malloc(sizeof(*args));
	args->from_trash = cfg.use_trash && use_trash;

	if(args->from_trash && path_starts_with(view->curr_dir, cfg.trash_dir))
	{
		show_error_msg("Can't perform deletion",
				"Current directory is under Trash directory");
		free(args);
		return 0;
	}

	args->list = NULL;
	args->nlines = 0;
	args->move = 0;
	args->force = 0;

	get_all_selected_files(view);

	i = view->list_pos;
	while(i < view->list_rows - 1 && view->dir_entry[i].selected)
		i++;

	view->list_pos = i;

	args->sel_list = view->selected_filelist;
	args->sel_list_len = view->selected_files;

	view->selected_filelist = NULL;
	free_selected_file_array(view);
	clean_selected_files(view);
	load_saving_pos(view, 1);

	strcpy(args->src, view->curr_dir);

	if(args->from_trash)
		snprintf(buf, sizeof(buf), "delete in %s: ",
				replace_home_part(view->curr_dir));
	else
		snprintf(buf, sizeof(buf), "Delete in %s: ",
				replace_home_part(view->curr_dir));

	get_group_file_list(view->selected_filelist, view->selected_files, buf);

#ifndef _WIN32
	args->job = add_background_job(-1, buf, -1);
#else
	args->job = add_background_job(-1, buf, (HANDLE)-1);
#endif
	if(args->job == NULL)
	{
		free_string_array(args->sel_list, args->sel_list_len);
		free(args);
		return 0;
	}

	args->job->total = args->sel_list_len;
	args->job->done = 0;

	pthread_create(&id, NULL, delete_file_stub, args);
	return 0;
}

static int
mv_file(const char *src, const char *src_path, const char *dst,
		const char *path, int tmpfile_num)
{
	char full_src[PATH_MAX], full_dst[PATH_MAX];
	int op;
	int result;

	snprintf(full_src, sizeof(full_src), "%s/%s", src_path, src);
	chosp(full_src);
	snprintf(full_dst, sizeof(full_dst), "%s/%s", path, dst);
	chosp(full_dst);

	/* compare case sensitive strings even on Windows to let user rename file
	 * changing only case of some characters */
	if(strcmp(full_src, full_dst) == 0)
		return 0;

	if(tmpfile_num <= 0)
		op = OP_MOVE;
	else if(tmpfile_num == 1)
		op = OP_MOVETMP1;
	else if(tmpfile_num == 2)
		op = OP_MOVETMP2;
	else if(tmpfile_num == 3)
		op = OP_MOVETMP3;
	else if(tmpfile_num == 4)
		op = OP_MOVETMP4;
	else
		op = OP_NONE;

	result = perform_operation(op, NULL, full_src, full_dst);
	if(result == 0 && tmpfile_num >= 0)
		add_operation(op, NULL, NULL, full_src, full_dst);
	return result;
}

static void
rename_file_cb(const char *new_name)
{
	char *filename = get_current_file_name(curr_view);
	char buf[MAX(COMMAND_GROUP_INFO_LEN, 10 + NAME_MAX + 1)];
	char new[NAME_MAX + 1];
	size_t len;
	int mv_res;
	char **filename_ptr;

	if(new_name == NULL || new_name[0] == '\0')
		return;

	if(contains_slash(new_name))
	{
		status_bar_error("Name can not contain slash");
		curr_stats.save_msg = 1;
		return;
	}

	len = strlen(filename);
	snprintf(new, sizeof(new), "%s%s%s%s", new_name,
			(rename_file_ext[0] == '\0') ? "" : ".", rename_file_ext,
			(filename[len - 1] == '/') ? "/" : "");

	if(check_file_rename(curr_view->curr_dir, filename, new, ST_DIALOG) <= 0)
	{
		return;
	}

	snprintf(buf, sizeof(buf), "rename in %s: %s to %s",
			replace_home_part(curr_view->curr_dir), filename, new);
	cmd_group_begin(buf);
	mv_res = mv_file(filename, curr_view->curr_dir, new, curr_view->curr_dir, 0);
	cmd_group_end();
	if(mv_res != 0)
	{
		show_error_msg("Rename Error", "Rename operation failed");
		return;
	}

	filename_ptr = &curr_view->dir_entry[curr_view->list_pos].name;
	(void)replace_string(filename_ptr, new);

	load_saving_pos(curr_view, 1);
}

static int
complete_filename_only(const char *str)
{
	filename_completion(str, CT_FILE_WOE);
	return 0;
}

void
rename_current_file(FileView *view, int name_only)
{
	char filename[NAME_MAX + 1];

	if(!check_if_dir_writable(DR_CURRENT, view->curr_dir))
		return;

	snprintf(filename, sizeof(filename), "%s", get_current_file_name(view));
	if(is_parent_dir(filename))
	{
		show_error_msg("Rename error",
				"You can't rename parent directory this way");
		return;
	}

	chosp(filename);

	if(name_only)
	{
		snprintf(rename_file_ext, sizeof(rename_file_ext), "%s",
				extract_extension(filename));
	}
	else
	{
		rename_file_ext[0] = '\0';
	}

	clean_selected_files(view);
	enter_prompt_mode(L"New name: ", filename, rename_file_cb,
			complete_filename_only, 1);
}

TSTATIC int
is_name_list_ok(int count, int nlines, char *list[], char *files[])
{
	int i;

	if(nlines < count)
	{
		status_bar_errorf("Not enough file names (%d/%d)", nlines, count);
		curr_stats.save_msg = 1;
		return 0;
	}

	if(nlines > count)
	{
		status_bar_errorf("Too many file names (%d/%d)", nlines, count);
		curr_stats.save_msg = 1;
		return 0;
	}

	for(i = 0; i < count; i++)
	{
		chomp(list[i]);

		if(files != NULL)
		{
			char *file_s = find_slashr(files[i]);
			char *list_s = find_slashr(list[i]);
			if(list_s != NULL || file_s != NULL)
			{
				if(list_s - list[i] != file_s - files[i] ||
						strnoscmp(files[i], list[i], list_s - list[i]) != 0)
				{
					if(file_s == NULL)
						status_bar_errorf("Name \"%s\" contains slash", list[i]);
					else
						status_bar_errorf("Won't move \"%s\" file", files[i]);
					curr_stats.save_msg = 1;
					return 0;
				}
			}
		}

		if(list[i][0] != '\0' && is_in_string_array(list, i, list[i]))
		{
			status_bar_errorf("Name \"%s\" duplicates", list[i]);
			curr_stats.save_msg = 1;
			return 0;
		}

		if(list[i][0] == '\0')
			continue;
	}

	return 1;
}

/* Returns number of renamed files. */
static int
perform_renaming(FileView *view, char **files, int *is_dup, int len,
		char **list)
{
	char buf[MAX(10 + NAME_MAX, COMMAND_GROUP_INFO_LEN) + 1];
	size_t buf_len;
	int i;
	int renamed = 0;

	buf_len = snprintf(buf, sizeof(buf), "rename in %s: ",
			replace_home_part(view->curr_dir));

	for(i = 0; i < len && buf_len < COMMAND_GROUP_INFO_LEN; i++)
	{
		if(buf[buf_len - 2] != ':')
		{
			strncat(buf, ", ", sizeof(buf) - buf_len - 1);
			buf_len = strlen(buf);
		}
		buf_len += snprintf(buf + buf_len, sizeof(buf) - buf_len, "%s to %s",
				files[i], list[i]);
	}

	cmd_group_begin(buf);

	for(i = 0; i < len; i++)
	{
		const char *unique_name;

		if(list[i][0] == '\0')
			continue;
		if(strcmp(list[i], files[i]) == 0)
			continue;
		if(!is_dup[i])
			continue;

		unique_name = make_name_unique(files[i]);
		if(mv_file(files[i], view->curr_dir, unique_name, view->curr_dir, 2) != 0)
		{
			cmd_group_end();
			if(!last_cmd_group_empty())
				undo_group();
			status_bar_error("Temporary rename error");
			curr_stats.save_msg = 1;
			return 0;
		}
		(void)replace_string(&files[i], unique_name);
	}

	for(i = 0; i < len; i++)
	{
		if(list[i][0] == '\0')
			continue;
		if(strcmp(list[i], files[i]) == 0)
			continue;

		if(mv_file(files[i], view->curr_dir, list[i], view->curr_dir,
				is_dup[i] ? 1 : 0) == 0)
		{
			int pos;

			renamed++;

			pos = find_file_pos_in_list(view, files[i]);
			if(pos == view->list_pos)
			{
				(void)replace_string(&view->dir_entry[pos].name, list[i]);
			}
		}
	}

	cmd_group_end();

	return renamed;
}

static void
rename_files_ind(FileView *view, char **files, int *is_dup, int len)
{
	char **list;
	int nlines;

	if(len == 0 || (list = edit_list(len, files, &nlines, 0)) == NULL)
	{
		status_bar_message("0 files renamed");
		curr_stats.save_msg = 1;
		return;
	}

	if(is_name_list_ok(len, nlines, list, files) &&
			is_rename_list_ok(files, is_dup, len, list))
	{
		const int renamed = perform_renaming(view, files, is_dup, len, list);
		if(renamed >= 0)
		{
			status_bar_messagef("%d file%s renamed", renamed,
					(renamed == 1) ? "" : "s");
			curr_stats.save_msg = 1;
		}
	}

	free_string_array(list, nlines);
}

static char **
add_files_to_list(const char *path, char **files, int *len)
{
	DIR* dir;
	struct dirent* dentry;
	const char* slash = "";

	if(!is_dir(path))
	{
		*len = add_to_string_array(&files, *len, 1, path);
		return files;
	}

	dir = opendir(path);
	if(dir == NULL)
		return files;

	if(path[strlen(path) - 1] != '/')
		slash = "/";

	while((dentry = readdir(dir)) != NULL)
	{
		char buf[PATH_MAX];

		if(stroscmp(dentry->d_name, ".") == 0)
			continue;
		else if(stroscmp(dentry->d_name, "..") == 0)
			continue;

		snprintf(buf, sizeof(buf), "%s%s%s", path, slash, dentry->d_name);
		files = add_files_to_list(buf, files, len);
	}

	closedir(dir);
	return files;
}

int
rename_files(FileView *view, char **list, int nlines, int recursive)
{
	char **files = NULL;
	int len;
	int i;
	int *is_dup;

	if(recursive && nlines != 0)
	{
		status_bar_error("Recursive rename doesn't accept list of new names");
		return 1;
	}

	if(!check_if_dir_writable(DR_CURRENT, view->curr_dir))
		return 0;

	if(view->selected_files == 0)
	{
		view->dir_entry[view->list_pos].selected = 1;
		view->selected_files = 1;
	}

	len = 0;
	for(i = 0; i < view->list_rows; i++)
	{
		if(!view->dir_entry[i].selected)
			continue;
		if(is_parent_dir(view->dir_entry[i].name))
			continue;
		if(recursive)
		{
			files = add_files_to_list(view->dir_entry[i].name, files, &len);
		}
		else
		{
			len = add_to_string_array(&files, len, 1, view->dir_entry[i].name);
			chosp(files[len - 1]);
		}
	}

	is_dup = calloc(len, sizeof(*is_dup));
	if(is_dup == NULL)
	{
		free_string_array(files, len);
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return 0;
	}

	if(nlines == 0)
	{
		rename_files_ind(view, files, is_dup, len);
	}
	else
	{
		int renamed = -1;

		if(is_name_list_ok(len, nlines, list, files) &&
				is_rename_list_ok(files, is_dup, len, list))
			renamed = perform_renaming(view, files, is_dup, len, list);

		if(renamed >= 0)
			status_bar_messagef("%d file%s renamed", renamed,
					(renamed == 1) ? "" : "s");
	}

	free_string_array(files, len);
	free(is_dup);

	clean_selected_files(view);
	redraw_view(view);
	curr_stats.save_msg = 1;
	return 1;
}

/* Checks rename correctness and forms an array of duplication marks.
 * Directory names in files array should be without trailing slash. */
TSTATIC int
is_rename_list_ok(char *files[], int *is_dup, int len, char *list[])
{
	int i;
	for(i = 0; i < len; i++)
	{
		int j;

		const int check_result =
			check_file_rename(curr_view->curr_dir, files[i], list[i], ST_NONE);
		if(check_result < 0)
		{
			continue;
		}

		for(j = 0; j < len; j++)
		{
			if(strcmp(list[i], files[j]) == 0 && !is_dup[j])
			{
				is_dup[j] = 1;
				break;
			}
		}
		if(j >= len && check_result == 0)
		{
			break;
		}
	}
	return i >= len;
}

static void
make_undo_string(FileView *view, char *buf, int nlines, char **list)
{
	int i;
	size_t len = strlen(buf);
	for(i = 0; i < view->selected_files && len < COMMAND_GROUP_INFO_LEN; i++)
	{
		if(buf[len - 2] != ':')
		{
			strncat(buf, ", ", COMMAND_GROUP_INFO_LEN - len - 1);
			len = strlen(buf);
		}
		strncat(buf, view->selected_filelist[i], COMMAND_GROUP_INFO_LEN - len - 1);
		len = strlen(buf);
		if(nlines > 0)
		{
			strncat(buf, " to ", COMMAND_GROUP_INFO_LEN - len - 1);
			len = strlen(buf);
			strncat(buf, list[i], COMMAND_GROUP_INFO_LEN - len - 1);
			len = strlen(buf);
		}
	}
}

/* Returns number of digets in passed number. */
static int
count_digits(int number)
{
	int result = 0;
	while(number != 0)
	{
		number /= 10;
		result++;
	}
	return MAX(1, result);
}

/* Returns pointer to a statically allocated buffer */
TSTATIC const char *
add_to_name(const char filename[], int k)
{
	static char result[NAME_MAX];
	char format[16];
	char *b, *e;
	int i, n;

	if((b = strpbrk(filename, "0123456789")) == NULL)
	{
		copy_str(result, sizeof(result), filename);
		return result;
	}

	n = 0;
	while(b[n] == '0' && isdigit(b[n + 1]))
	{
		n++;
	}

	if(b != filename && b[-1] == '-')
	{
		b--;
	}

	i = strtol(b, &e, 10);

	if(i + k < 0)
	{
		n++;
	}

	snprintf(result, b - filename + 1, "%s", filename);
	snprintf(format, sizeof(format), "%%0%dd%%s", n + count_digits(i));
	snprintf(result + (b - filename), sizeof(result) - (b - filename), format,
			i + k, e);

	return result;
}

/* Returns new value for save_msg flag. */
int
incdec_names(FileView *view, int k)
{
	size_t names_len;
	char **names;
	size_t tmp_len = 0;
	char **tmp_names = NULL;
	char buf[MAX(NAME_MAX, COMMAND_GROUP_INFO_LEN)];
	int i;
	int err = 0;
	int renames = 0;

	get_all_selected_files(view);
	names_len = view->selected_files;
	names = copy_string_array(view->selected_filelist, names_len);

	snprintf(buf, sizeof(buf), "<c-a> in %s: ",
			replace_home_part(view->curr_dir));
	make_undo_string(view, buf, 0, NULL);

	if(!view->user_selection)
		clean_selected_files(view);

	for(i = 0; i < names_len; i++)
	{
		if(strpbrk(names[i], "0123456789") == NULL)
		{
			remove_from_string_array(names, names_len--, i--);
			continue;
		}
		chosp(names[i]);
		tmp_len = add_to_string_array(&tmp_names, tmp_len, 1,
				make_name_unique(names[i]));
	}

	for(i = 0; i < names_len; i++)
	{
		const char *p = add_to_name(names[i], k);
#ifndef _WIN32
		if(is_in_string_array(names, names_len, p))
#else
		if(is_in_string_array_case(names, names_len, p))
#endif
			continue;
		if(check_file_rename(view->curr_dir, names[i], p, ST_STATUS_BAR) != 0)
			continue;

		err = -1;
		break;
	}

	cmd_group_begin(buf);
	for(i = 0; i < names_len && !err; i++)
	{
		if(mv_file(names[i], view->curr_dir, tmp_names[i], view->curr_dir, 4) != 0)
		{
			err = 1;
			break;
		}
		renames++;
	}
	for(i = 0; i < names_len && !err; i++)
	{
		if(mv_file(tmp_names[i], view->curr_dir, add_to_name(names[i], k),
				view->curr_dir, 3) != 0)
		{
			err = 1;
			break;
		}
		renames++;
	}
	cmd_group_end();

	free_string_array(names, names_len);
	free_string_array(tmp_names, tmp_len);

	if(err)
	{
		if(err > 0 && !last_cmd_group_empty())
			undo_group();
	}
	else if(view->dir_entry[view->list_pos].selected || !view->user_selection)
	{
		char **filename = &view->dir_entry[view->list_pos].name;
		(void)replace_string(filename, add_to_name(*filename, k));
	}

	clean_selected_files(view);
	if(renames > 0)
	{
		load_saving_pos(view, 0);
	}

	if(err > 0)
	{
		status_bar_error("Rename error");
	}
	else if(err == 0)
	{
		status_bar_messagef("%d file%s renamed", names_len,
				(names_len == 1) ? "" : "s");
	}

	return 1;
}

/* Returns value > 0 if rename is correct, < 0 if rename isn't needed and 0
 * when rename operation should be aborted. silent parameter controls whether
 * error dialog or status bar message should be shown, 0 means dialog. */
TSTATIC int
check_file_rename(const char dir[], const char old[], const char new[],
		SignalType signal_type)
{
	if(!is_file_name_changed(old, new))
	{
		return -1;
	}

	if(path_exists_at(dir, new) && stroscmp(old, new) != 0)
	{
		switch(signal_type)
		{
			case ST_STATUS_BAR:
				status_bar_errorf("File \"%s\" already exists", new);
				curr_stats.save_msg = 1;
				break;
			case ST_DIALOG:
				show_error_msg("File exists",
						"That file already exists. Will not overwrite.");
				break;

			default:
				assert(signal_type == ST_NONE && "Unhandled signaling type");
				break;
		}
		return 0;
	}

	return 1;
}

/* Checks whether file name change was performed.  Returns non-zero if change is
 * detected, otherwise zero is returned. */
static int
is_file_name_changed(const char old[], const char new[])
{
	/* Empty new name means reuse of the old name (rename cancellation).  Names
	 * are always compared in a case sensitive way, so that changes in case of
	 * letters triggers rename operation even for systems where paths are case
	 * insensitive. */
	return (new[0] != '\0' && strcmp(old, new) != 0);
}

#ifndef _WIN32
void
chown_files(int u, int g, uid_t uid, gid_t gid)
{
	char buf[COMMAND_GROUP_INFO_LEN + 1];
	int i;

	snprintf(buf, sizeof(buf), "ch%s in %s: ", ((u && g) || u) ? "own" : "grp",
			replace_home_part(curr_view->curr_dir));

	get_group_file_list(curr_view->saved_selection, curr_view->nsaved_selection,
			buf);
	cmd_group_begin(buf);
	for(i = 0; i < curr_view->nsaved_selection; i++)
	{
		char *filename = curr_view->saved_selection[i];
		int pos = find_file_pos_in_list(curr_view, filename);

		if(u && perform_operation(OP_CHOWN, (void *)(long)uid, filename, NULL) == 0)
			add_operation(OP_CHOWN, (void *)(long)uid,
					(void *)(long)curr_view->dir_entry[pos].uid, filename, "");
		if(g && perform_operation(OP_CHGRP, (void *)(long)gid, filename, NULL) == 0)
			add_operation(OP_CHGRP, (void *)(long)gid,
					(void *)(long)curr_view->dir_entry[pos].gid, filename, "");
	}
	cmd_group_end();

	load_dir_list(curr_view, 1);
	move_to_list_pos(curr_view, curr_view->list_pos);
}
#endif

static void
change_owner_cb(const char *new_owner)
{
#ifndef _WIN32
	uid_t uid;

	if(new_owner == NULL || new_owner[0] == '\0')
		return;

	if(get_uid(new_owner, &uid) != 0)
	{
		status_bar_errorf("Invalid user name: \"%s\"", new_owner);
		curr_stats.save_msg = 1;
		return;
	}

	chown_files(1, 0, uid, 0);
#endif
}

#ifndef _WIN32
static int
complete_owner(const char *str)
{
	complete_user_name(str);
	return 0;
}
#endif

void
change_owner(void)
{
	if(curr_view->selected_filelist == 0)
	{
		curr_view->dir_entry[curr_view->list_pos].selected = 1;
		curr_view->selected_files = 1;
	}
	clean_selected_files(curr_view);
#ifndef _WIN32
	enter_prompt_mode(L"New owner: ", "", change_owner_cb, &complete_owner, 0);
#else
	enter_prompt_mode(L"New owner: ", "", change_owner_cb, NULL, 0);
#endif
}

static void
change_group_cb(const char *new_group)
{
#ifndef _WIN32
	gid_t gid;

	if(new_group == NULL || new_group[0] == '\0')
		return;

	if(get_gid(new_group, &gid) != 0)
	{
		status_bar_errorf("Invalid group name: \"%s\"", new_group);
		curr_stats.save_msg = 1;
		return;
	}

	chown_files(0, 1, 0, gid);
#endif
}

#ifndef _WIN32
static int
complete_group(const char *str)
{
	complete_group_name(str);
	return 0;
}
#endif

void
change_group(void)
{
	if(curr_view->selected_filelist == 0)
	{
		curr_view->dir_entry[curr_view->list_pos].selected = 1;
		curr_view->selected_files = 1;
	}
	clean_selected_files(curr_view);
#ifndef _WIN32
	enter_prompt_mode(L"New group: ", "", change_group_cb, &complete_group, 0);
#else
	enter_prompt_mode(L"New group: ", "", change_group_cb, NULL, 0);
#endif
}

static void
change_link_cb(const char *new_target)
{
	char buf[MAX(COMMAND_GROUP_INFO_LEN, PATH_MAX)];
	char linkto[PATH_MAX];
	const char *filename;

	if(new_target == NULL || new_target[0] == '\0')
		return;

	curr_stats.confirmed = 1;

	filename = curr_view->dir_entry[curr_view->list_pos].name;
	if(get_link_target(filename, linkto, sizeof(linkto)) != 0)
	{
		show_error_msg("Error", "Can't read link");
		return;
	}

	snprintf(buf, sizeof(buf), "cl in %s: on %s from \"%s\" to \"%s\"",
			replace_home_part(curr_view->curr_dir), filename, linkto, new_target);
	cmd_group_begin(buf);

	snprintf(buf, sizeof(buf), "%s/%s", curr_view->curr_dir, filename);
	chosp(buf);

	if(perform_operation(OP_REMOVESL, NULL, buf, NULL) == 0)
		add_operation(OP_REMOVESL, NULL, NULL, buf, linkto);
	if(perform_operation(OP_SYMLINK2, NULL, new_target, buf) == 0)
		add_operation(OP_SYMLINK2, NULL, NULL, new_target, buf);

	cmd_group_end();
}

static int
complete_filename(const char *str)
{
	const char *name_begin = after_last(str, '/');
	filename_completion(str, CT_ALL_WOE);
	return name_begin - str;
}

int
change_link(FileView *view)
{
	char linkto[PATH_MAX];

	if(!symlinks_available())
	{
		show_error_msg("Symbolic Links Error",
				"Your OS doesn't support symbolic links");
		return 0;
	}

	if(!check_if_dir_writable(DR_CURRENT, view->curr_dir))
		return 0;

	if(view->dir_entry[view->list_pos].type != LINK)
	{
		status_bar_error("File isn't a symbolic link");
		return 1;
	}

	if(get_link_target(view->dir_entry[view->list_pos].name, linkto,
			sizeof(linkto)) != 0)
	{
		show_error_msg("Error", "Can't read link");
		return 0;
	}

	enter_prompt_mode(L"Link target: ", linkto, change_link_cb,
			&complete_filename, 0);
	return 0;
}

static void
prompt_dest_name(const char *src_name)
{
	wchar_t buf[256];

	my_swprintf(buf, ARRAY_LEN(buf), L"New name for %" WPRINTF_MBSTR L": ",
			src_name);
	enter_prompt_mode(buf, src_name, put_confirm_cb, NULL, 0);
}

/* Returns 0 on success, otherwise non-zero is returned. */
static int
put_next(const char dest_name[], int override)
{
	char *filename;
	struct stat src_st;
	char src_buf[PATH_MAX], dst_buf[PATH_MAX];
	int from_trash;
	int op;
	int move;

	/* TODO: refactor this function (put_next()) */

	override = override || put_confirm.overwrite_all;

	filename = put_confirm.reg->files[put_confirm.x];
	chosp(filename);
	if(lstat(filename, &src_st) != 0)
		return 0;

	from_trash = strnoscmp(filename, cfg.trash_dir, strlen(cfg.trash_dir)) == 0;
	move = from_trash || put_confirm.force_move;

	if(dest_name[0] == '\0')
		dest_name = find_slashr(filename) + 1;

	copy_str(src_buf, sizeof(src_buf), filename);
	chosp(src_buf);

	if(from_trash)
	{
		while(isdigit(*dest_name))
			dest_name++;
		dest_name++;
	}

	snprintf(dst_buf, sizeof(dst_buf), "%s/%s", put_confirm.view->curr_dir,
			dest_name);
	chosp(dst_buf);

	if(path_exists(dst_buf) && !override)
	{
		struct stat dst_st;
		put_confirm.allow_merge = lstat(dst_buf, &dst_st) == 0 &&
				S_ISDIR(dst_st.st_mode) && S_ISDIR(src_st.st_mode);
		prompt_what_to_do(dest_name);
		return 1;
	}

	if(override)
	{
		struct stat dst_st;
		if(lstat(dst_buf, &dst_st) == 0 && (!put_confirm.merge ||
				S_ISDIR(dst_st.st_mode) != S_ISDIR(src_st.st_mode)))
		{
			if(perform_operation(OP_REMOVESL, NULL, dst_buf, NULL) != 0)
			{
				return 0;
			}
		}
		else
		{
#ifndef _WIN32
			remove_last_path_component(dst_buf);
#endif
		}

		request_view_update(put_confirm.view);
	}

	if(put_confirm.link)
	{
		op = OP_SYMLINK;
		if(put_confirm.link == 2)
		{
			copy_str(src_buf, sizeof(src_buf),
					make_rel_path(filename, put_confirm.view->curr_dir));
		}
	}
	else if(move)
	{
		op = put_confirm.merge ? OP_MOVEF : OP_MOVE;
	}
	else
	{
		op = put_confirm.merge ? OP_COPYF : OP_COPY;
	}

	progress_msg("Putting files", put_confirm.x + 1, put_confirm.reg->num_files);
	if(perform_operation(op, NULL, src_buf, dst_buf) == 0)
	{
		char *msg, *p;
		size_t len;

		cmd_group_continue();

		msg = replace_group_msg(NULL);
		len = strlen(msg);
		p = realloc(msg, COMMAND_GROUP_INFO_LEN);
		if(p == NULL)
			len = COMMAND_GROUP_INFO_LEN;
		else
			msg = p;

		snprintf(msg + len, COMMAND_GROUP_INFO_LEN - len, "%s%s",
				(msg[len - 2] != ':') ? ", " : "", dest_name);
		replace_group_msg(msg);
		free(msg);

		add_operation(op, NULL, NULL, src_buf, dst_buf);

		cmd_group_end();
		put_confirm.y++;
		if(move)
		{
			free(put_confirm.reg->files[put_confirm.x]);
			put_confirm.reg->files[put_confirm.x] = NULL;
		}
	}

	return 0;
}

static void
put_confirm_cb(const char *dest_name)
{
	if(dest_name == NULL || dest_name[0] == '\0')
		return;

	if(put_next(dest_name, 0) == 0)
	{
		put_confirm.x++;
		curr_stats.save_msg = put_files_from_register_i(put_confirm.view, 0);
	}
}

static void
put_decide_cb(const char *choice)
{
	if(choice == NULL || choice[0] == '\0' || strcmp(choice, "r") == 0)
	{
		prompt_dest_name(put_confirm.name);
	}
	else if(strcmp(choice, "s") == 0)
	{
		put_confirm.x++;
		curr_stats.save_msg = put_files_from_register_i(put_confirm.view, 0);
	}
	else if(strcmp(choice, "o") == 0)
	{
		if(put_next("", 1) == 0)
		{
			put_confirm.x++;
			curr_stats.save_msg = put_files_from_register_i(put_confirm.view, 0);
		}
	}
	else if(strcmp(choice, "a") == 0)
	{
		put_confirm.overwrite_all = 1;
		if(put_next("", 1) == 0)
		{
			put_confirm.x++;
			curr_stats.save_msg = put_files_from_register_i(put_confirm.view, 0);
		}
	}
	else if(put_confirm.allow_merge && strcmp(choice, "m") == 0)
	{
		put_confirm.merge = 1;
		if(put_next("", 1) == 0)
		{
			put_confirm.x++;
			curr_stats.save_msg = put_files_from_register_i(put_confirm.view, 0);
		}
	}
	else
	{
		prompt_what_to_do(put_confirm.name);
	}
}

/* Prompt user for conflict resolution strategy about given filename. */
static void
prompt_what_to_do(const char src_name[])
{
	wchar_t buf[NAME_MAX];

	(void)replace_string(&put_confirm.name, src_name);
	my_swprintf(buf, ARRAY_LEN(buf), L"Name conflict for %" WPRINTF_MBSTR
			L". [r]ename/[s]kip/[o]verwrite/overwrite [a]ll%" WPRINTF_MBSTR ": ",
			src_name, put_confirm.allow_merge ? "/[m]erge" : "");
	enter_prompt_mode(buf, "", put_decide_cb, NULL, 0);
}

/* Returns new value for save_msg flag. */
int
put_files_from_register(FileView *view, int name, int force_move)
{
	registers_t *reg;

	if(!check_if_dir_writable(DR_CURRENT, view->curr_dir))
		return 0;

	reg = find_register(tolower(name));

	if(reg == NULL || reg->num_files < 1)
	{
		status_bar_error("Register is empty");
		return 1;
	}

	put_confirm.reg = reg;
	put_confirm.force_move = force_move;
	put_confirm.x = 0;
	put_confirm.y = 0;
	put_confirm.view = view;
	put_confirm.overwrite_all = 0;
	put_confirm.link = 0;
	put_confirm.merge = 0;
	put_confirm.allow_merge = 0;
	return put_files_from_register_i(view, 1);
}

TSTATIC const char *
gen_clone_name(const char normal_name[])
{
	static char result[NAME_MAX];

	char extension[NAME_MAX];
	int i;
	size_t len;
	char *p;

	snprintf(result, sizeof(result), "%s", normal_name);
	chosp(result);

	snprintf(extension, sizeof(extension), "%s", extract_extension(result));

	len = strlen(result);
	i = 1;
	if(result[len - 1] == ')' && (p = strrchr(result, '(')) != NULL)
	{
		char *t;
		long l;
		if((l = strtol(p + 1, &t, 10)) > 0 && t[1] == '\0')
		{
			len = p - result;
			i = l + 1;
		}
	}

	do
	{
		snprintf(result + len, sizeof(result) - len, "(%d)%s%s", i++,
				(extension[0] == '\0') ? "" : ".", extension);
	}
	while(path_exists(result));

	return result;
}

static void
clone_file(FileView* view, const char *filename, const char *path,
		const char *clone)
{
	char full[PATH_MAX];
	char clone_name[PATH_MAX];
	
	if(stroscmp(filename, "./") == 0)
		return;
	if(is_parent_dir(filename))
		return;

	snprintf(clone_name, sizeof(clone_name), "%s/%s", path, clone);
	chosp(clone_name);
	if(path_exists(clone_name))
	{
		if(perform_operation(OP_REMOVESL, NULL, clone_name, NULL) != 0)
			return;
	}

	snprintf(full, sizeof(full), "%s/%s", view->curr_dir, filename);
	chosp(full);

	if(perform_operation(OP_COPY, NULL, full, clone_name) == 0)
		add_operation(OP_COPY, NULL, NULL, full, clone_name);
}

static int
is_clone_list_ok(int count, char **list)
{
	int i;
	for(i = 0; i < count; i++)
	{
		if(path_exists(list[i]))
		{
			status_bar_errorf("File \"%s\" already exists", list[i]);
			return 0;
		}
	}
	return 1;
}

static int
is_dir_path(FileView *view, const char *path, char *buf)
{
	strcpy(buf, view->curr_dir);

	if(path[0] == '/' || path[0] == '~')
	{
		char *expanded_path = expand_tilde(strdup(path));
		strcpy(buf, expanded_path);
		free(expanded_path);
	}
	else
	{
		strcat(buf, "/");
		strcat(buf, path);
	}

	if(is_dir(buf))
		return 1;

	strcpy(buf, view->curr_dir);
	return 0;
}

/* returns new value for save_msg */
int
clone_files(FileView *view, char **list, int nlines, int force, int copies)
{
	int i;
	char buf[COMMAND_GROUP_INFO_LEN + 1];
	char path[PATH_MAX];
	int with_dir = 0;
	int from_file;
	char **sel;
	int sel_len;

	if(!have_read_access(view))
		return 0;

	if(nlines == 1)
	{
		if((with_dir = is_dir_path(view, list[0], path)))
			nlines = 0;
	}
	else
	{
		strcpy(path, view->curr_dir);
	}
	if(!check_if_dir_writable(with_dir ? DR_DESTINATION : DR_CURRENT, path))
		return 0;

	get_all_selected_files(view);

	from_file = nlines < 0;
	if(from_file)
	{
		list = edit_list(view->selected_files, view->selected_filelist, &nlines, 0);
		if(list == NULL)
		{
			free_selected_file_array(view);
			return 0;
		}
	}

	if(nlines > 0 &&
			(!is_name_list_ok(view->selected_files, nlines, list, NULL) ||
			(!force && !is_clone_list_ok(nlines, list))))
	{
		clean_selected_files(view);
		redraw_view(view);
		if(from_file)
			free_string_array(list, nlines);
		return 1;
	}

	if(with_dir)
		snprintf(buf, sizeof(buf), "clone in %s to %s: ", view->curr_dir, list[0]);
	else
		snprintf(buf, sizeof(buf), "clone in %s: ", view->curr_dir);
	make_undo_string(view, buf, nlines, list);

	sel_len = view->selected_files;
	sel = copy_string_array(view->selected_filelist, sel_len);
	if(!view->user_selection)
	{
		free_selected_file_array(view);
		for(i = 0; i < view->list_rows; i++)
			view->dir_entry[i].selected = 0;
		view->selected_files = 0;
	}

	cmd_group_begin(buf);
	for(i = 0; i < sel_len; i++)
	{
		int j;
		const char * clone_name;
		if(nlines > 0)
		{
			clone_name = list[i];
		}
		else
		{
			clone_name = path_exists_at(path, sel[i]) ? gen_clone_name(sel[i]) :
				sel[i];
		}
		progress_msg("Cloning files", i + 1, sel_len);

		for(j = 0; j < copies; j++)
		{
			if(path_exists_at(path, clone_name))
				clone_name = gen_clone_name((nlines > 0) ? list[i] : sel[i]);
			clone_file(view, sel[i], path, clone_name);
		}

		if(find_file_pos_in_list(view, sel[i]) == view->list_pos)
		{
			free(view->dir_entry[view->list_pos].name);
			view->dir_entry[view->list_pos].name = malloc(strlen(clone_name) + 2);
			strcpy(view->dir_entry[view->list_pos].name, clone_name);
			if(ends_with_slash(sel[i]))
				strcat(view->dir_entry[view->list_pos].name, "/");
		}
	}
	cmd_group_end();
	free_selected_file_array(view);
	free_string_array(sel, sel_len);

	clean_selected_files(view);
	load_saving_pos(view, 1);
	load_saving_pos(other_view, 1);
	if(from_file)
		free_string_array(list, nlines);
	return 0;
}

static void
set_dir_size(const char *path, uint64_t size)
{
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

	pthread_mutex_lock(&mutex);
	tree_set_data(curr_stats.dirsize_cache, path, size);
	pthread_mutex_unlock(&mutex);
}

uint64_t
calc_dirsize(const char *path, int force_update)
{
	DIR* dir;
	struct dirent* dentry;
	const char* slash = "";
	uint64_t size;

	dir = opendir(path);
	if(dir == NULL)
		return 0;

	if(path[strlen(path) - 1] != '/')
		slash = "/";

	size = 0;
	while((dentry = readdir(dir)) != NULL)
	{
		char buf[PATH_MAX];

		if(stroscmp(dentry->d_name, ".") == 0)
			continue;
		else if(stroscmp(dentry->d_name, "..") == 0)
			continue;

		snprintf(buf, sizeof(buf), "%s%s%s", path, slash, dentry->d_name);
		if(entry_is_dir(buf, dentry))
		{
			uint64_t dir_size = 0;
			if(tree_get_data(curr_stats.dirsize_cache, buf, &dir_size) != 0
					|| force_update)
				dir_size = calc_dirsize(buf, force_update);
			size += dir_size;
		}
		else
		{
			size += get_file_size(buf);
		}
	}

	closedir(dir);

	set_dir_size(path, size);
	return size;
}

/* Uses dentry to check file type and fallbacks to lstat() if dentry contains
 * unknown type. */
static int
entry_is_dir(const char full_path[], const struct dirent* dentry)
{
#ifndef _WIN32
		struct stat s;
		if(dentry->d_type != DT_UNKNOWN)
		{
			return dentry->d_type == DT_DIR;
		}
		if(lstat(full_path, &s) == 0 && s.st_ino != 0)
		{
			return (s.st_mode&S_IFMT) == S_IFDIR;
		}
		return 0;
#else
		return is_dir(full_path);
#endif
}

/* Returns new value for save_msg flag. */
int
put_links(FileView *view, int reg_name, int relative)
{
	registers_t *reg;

	if(!check_if_dir_writable(DR_CURRENT, view->curr_dir))
		return 0;

	reg = find_register(reg_name);

	if(reg == NULL || reg->num_files < 1)
	{
		status_bar_error("Register is empty");
		return 1;
	}

	put_confirm.reg = reg;
	put_confirm.force_move = 0;
	put_confirm.x = 0;
	put_confirm.y = 0;
	put_confirm.view = view;
	put_confirm.overwrite_all = 0;
	put_confirm.link = relative ? 2 : 1;
	put_confirm.merge = 0;
	put_confirm.allow_merge = 0;
	return put_files_from_register_i(view, 1);
}

/* Returns new value for save_msg flag. */
static int
put_files_from_register_i(FileView *view, int start)
{
	if(start)
	{
		char buf[MAX(COMMAND_GROUP_INFO_LEN, PATH_MAX + NAME_MAX*2 + 4)];
		const char *op = "UNKNOWN";
		int from_trash = strnoscmp(put_confirm.reg->files[0], cfg.trash_dir,
				strlen(cfg.trash_dir)) == 0;
		if(put_confirm.link == 0)
			op = (put_confirm.force_move || from_trash) ? "Put" : "put";
		else if(put_confirm.link == 1)
			op = "put absolute links";
		else if(put_confirm.link == 2)
			op = "put relative links";
		snprintf(buf, sizeof(buf), "%s in %s: ", op,
				replace_home_part(view->curr_dir));
		cmd_group_begin(buf);
		cmd_group_end();
	}

	if(my_chdir(view->curr_dir) != 0)
	{
		show_error_msg("Directory Return", "Can't chdir() to current directory");
		return 1;
	}
	while(put_confirm.x < put_confirm.reg->num_files)
	{
		if(put_next("", 0) != 0)
			return 0;
		put_confirm.x++;
	}

	pack_register(put_confirm.reg->name);

	status_bar_messagef("%d file%s inserted", put_confirm.y,
			(put_confirm.y == 1) ? "" : "s");

	load_saving_pos(put_confirm.view, 1);

	return 1;
}

/* off can be NULL */
static const char *
substitute_regexp(const char *src, const char *sub, const regmatch_t *matches,
		int *off)
{
	static char buf[NAME_MAX];
	char *dst = buf;
	int i;

	for(i = 0; i < matches[0].rm_so; i++)
		*dst++ = src[i];

	while(*sub != '\0')
	{
		if(*sub == '\\')
		{
			if(sub[1] == '\0')
				break;
			else if(isdigit(sub[1]))
			{
				int n = sub[1] - '0';
				for(i = matches[n].rm_so; i < matches[n].rm_eo; i++)
					*dst++ = src[i];
				sub += 2;
				continue;
			}
			else
				sub++;
		}
		*dst++ = *sub++;
	}
	if(off != NULL)
		*off = dst - buf;

	for(i = matches[0].rm_eo; src[i] != '\0'; i++)
		*dst++ = src[i];

	*dst = '\0';

	return buf;
}

/* Returns pointer to a statically allocated buffer. */
static const char *
gsubstitute_regexp(regex_t *re, const char src[], const char sub[],
		regmatch_t matches[])
{
	static char buf[NAME_MAX];
	int off = 0;

	copy_str(buf, sizeof(buf), src);
	do
	{
		int i;
		for(i = 0; i < 10; i++)
		{
			matches[i].rm_so += off;
			matches[i].rm_eo += off;
		}

		src = substitute_regexp(buf, sub, matches, &off);
		copy_str(buf, sizeof(buf), src);

		if(matches[0].rm_eo == matches[0].rm_so)
			break;
	}
	while(regexec(re, buf + off, 10, matches, 0) == 0);

	return buf;
}

const char *
substitute_in_name(const char name[], const char pattern[], const char sub[],
		int glob)
{
	static char buf[PATH_MAX];
	regex_t re;
	regmatch_t matches[10];
	const char *dst;

	copy_str(buf, sizeof(buf), name);

	if(regcomp(&re, pattern, REG_EXTENDED) != 0)
	{
		regfree(&re);
		return buf;
	}

	if(regexec(&re, name, ARRAY_LEN(matches), matches, 0) != 0)
	{
		regfree(&re);
		return buf;
	}

	if(glob && pattern[0] != '^')
		dst = gsubstitute_regexp(&re, name, sub, matches);
	else
		dst = substitute_regexp(name, sub, matches, NULL);
	copy_str(buf, sizeof(buf), dst);

	regfree(&re);
	return buf;
}

static int
change_in_names(FileView *view, char c, const char *pattern, const char *sub,
		char **dest)
{
	int i, j;
	int n;
	char buf[COMMAND_GROUP_INFO_LEN + 1];
	size_t len;

	len = snprintf(buf, sizeof(buf), "%c/%s/%s/ in %s: ", c, pattern, sub,
			replace_home_part(view->curr_dir));

	for(i = 0; i < view->selected_files && len < COMMAND_GROUP_INFO_LEN; i++)
	{
		if(!view->dir_entry[i].selected)
			continue;
		if(is_parent_dir(view->dir_entry[i].name))
			continue;

		if(buf[len - 2] != ':')
		{
			strncat(buf, ", ", sizeof(buf) - len - 1);
			len = strlen(buf);
		}
		strncat(buf, view->dir_entry[i].name, sizeof(buf) - len - 1);
		len = strlen(buf);
	}
	cmd_group_begin(buf);
	n = 0;
	j = -1;
	for(i = 0; i < view->list_rows; i++)
	{
		char buf[NAME_MAX];

		if(!view->dir_entry[i].selected)
			continue;
		if(is_parent_dir(view->dir_entry[i].name))
			continue;

		strncpy(buf, view->dir_entry[i].name, sizeof(buf));
		chosp(buf);
		j++;
		if(strcmp(buf, dest[j]) == 0)
			continue;

		if(i == view->list_pos)
		{
			(void)replace_string(&view->dir_entry[i].name, dest[j]);
		}

		if(mv_file(buf, view->curr_dir, dest[j], view->curr_dir, 0) == 0)
			n++;
	}
	cmd_group_end();
	free_string_array(dest, j + 1);
	status_bar_messagef("%d file%s renamed", n, (n == 1) ? "" : "s");
	return 1;
}

/* Returns new value for save_msg flag. */
int
substitute_in_names(FileView *view, const char *pattern, const char *sub,
		int ic, int glob)
{
	int i;
	regex_t re;
	char **dest = NULL;
	int n = 0;
	int cflags;
	int err;

	if(!check_if_dir_writable(DR_CURRENT, view->curr_dir))
		return 0;

	if(view->selected_files == 0)
	{
		view->dir_entry[view->list_pos].selected = 1;
		view->selected_files = 1;
	}

	if(ic == 0)
		cflags = get_regexp_cflags(pattern);
	else if(ic > 0)
		cflags = REG_EXTENDED | REG_ICASE;
	else
		cflags = REG_EXTENDED;
	if((err = regcomp(&re, pattern, cflags)) != 0)
	{
		status_bar_errorf("Regexp error: %s", get_regexp_error(err, &re));
		regfree(&re);
		return 1;
	}

	for(i = 0; i < view->list_rows; i++)
	{
		char buf[NAME_MAX];
		const char *dst;
		regmatch_t matches[10];
		struct stat st;

		if(!view->dir_entry[i].selected)
			continue;
		if(is_parent_dir(view->dir_entry[i].name))
			continue;

		strncpy(buf, view->dir_entry[i].name, sizeof(buf));
		chosp(buf);
		if(regexec(&re, buf, ARRAY_LEN(matches), matches, 0) != 0)
		{
			view->dir_entry[i].selected = 0;
			view->selected_files--;
			continue;
		}
		if(glob)
			dst = gsubstitute_regexp(&re, buf, sub, matches);
		else
			dst = substitute_regexp(buf, sub, matches, NULL);
		if(strcmp(buf, dst) == 0)
		{
			view->dir_entry[i].selected = 0;
			view->selected_files--;
			continue;
		}
		n = add_to_string_array(&dest, n, 1, dst);
		if(is_in_string_array(dest, n - 1, dst))
		{
			regfree(&re);
			free_string_array(dest, n);
			status_bar_errorf("Name \"%s\" duplicates", dst);
			return 1;
		}
		if(dst[0] == '\0')
		{
			regfree(&re);
			free_string_array(dest, n);
			status_bar_errorf("Destination name of \"%s\" is empty", buf);
			return 1;
		}
		if(contains_slash(dst))
		{
			regfree(&re);
			free_string_array(dest, n);
			status_bar_errorf("Destination name \"%s\" contains slash", dst);
			return 1;
		}
		if(lstat(dst, &st) == 0)
		{
			regfree(&re);
			free_string_array(dest, n);
			status_bar_errorf("File \"%s\" already exists", dst);
			return 1;
		}
	}
	regfree(&re);

	return change_in_names(view, 's', pattern, sub, dest);
}

static const char *
substitute_tr(const char *name, const char *pattern, const char *sub)
{
	static char buf[NAME_MAX];
	char *p = buf;
	while(*name != '\0')
	{
		const char *t = strchr(pattern, *name);
		if(t != NULL)
			*p++ = sub[t - pattern];
		else
			*p++ = *name;
		name++;
	}
	*p = '\0';
	return buf;
}

int
tr_in_names(FileView *view, const char *pattern, const char *sub)
{
	int i;
	char **dest = NULL;
	int n = 0;

	if(!check_if_dir_writable(DR_CURRENT, view->curr_dir))
		return 0;

	if(view->selected_files == 0)
	{
		view->dir_entry[view->list_pos].selected = 1;
		view->selected_files = 1;
	}

	for(i = 0; i < view->list_rows; i++)
	{
		char buf[NAME_MAX];
		const char *dst;
		struct stat st;

		if(!view->dir_entry[i].selected)
			continue;
		if(is_parent_dir(view->dir_entry[i].name))
			continue;

		strncpy(buf, view->dir_entry[i].name, sizeof(buf));
		chosp(buf);
		dst = substitute_tr(buf, pattern, sub);
		if(strcmp(buf, dst) == 0)
		{
			view->dir_entry[i].selected = 0;
			view->selected_files--;
			continue;
		}
		n = add_to_string_array(&dest, n, 1, dst);
		if(is_in_string_array(dest, n - 1, dst))
		{
			free_string_array(dest, n);
			status_bar_errorf("Name \"%s\" duplicates", dst);
			return 1;
		}
		if(dst[0] == '\0')
		{
			free_string_array(dest, n);
			status_bar_errorf("Destination name of \"%s\" is empty", buf);
			return 1;
		}
		if(contains_slash(dst))
		{
			free_string_array(dest, n);
			status_bar_errorf("Destination name \"%s\" contains slash", dst);
			return 1;
		}
		if(lstat(dst, &st) == 0)
		{
			free_string_array(dest, n);
			status_bar_errorf("File \"%s\" already exists", dst);
			return 1;
		}
	}

	return change_in_names(view, 't', pattern, sub, dest);
}

static void
str_tolower(char *str)
{
	while(*str != '\0')
	{
		*str = tolower(*str);
		str++;
	}
}

static void
str_toupper(char *str)
{
	while(*str != '\0')
	{
		*str = toupper(*str);
		str++;
	}
}

int
change_case(FileView *view, int toupper, int count, int indexes[])
{
	int i;
	char **dest = NULL;
	int n = 0, k;
	char buf[COMMAND_GROUP_INFO_LEN + 1];

	if(!check_if_dir_writable(DR_CURRENT, view->curr_dir))
		return 0;

	if(count > 0)
		get_selected_files(view, count, indexes);
	else
		get_all_selected_files(view);

	if(view->selected_files == 0)
	{
		status_bar_message("0 files renamed");
		return 1;
	}

	for(i = 0; i < view->selected_files; i++)
	{
		char buf[NAME_MAX];
		struct stat st;

		chosp(view->selected_filelist[i]);
		copy_str(buf, sizeof(buf), view->selected_filelist[i]);
		if(toupper)
			str_toupper(buf);
		else
			str_tolower(buf);

		n = add_to_string_array(&dest, n, 1, buf);

		if(is_in_string_array(dest, n - 1, buf))
		{
			free_string_array(dest, n);
			free_selected_file_array(view);
			view->selected_files = 0;
			status_bar_errorf("Name \"%s\" duplicates", buf);
			return 1;
		}
		if(strcmp(dest[i], buf) == 0)
			continue;
		if(lstat(buf, &st) == 0)
		{
			free_string_array(dest, n);
			free_selected_file_array(view);
			view->selected_files = 0;
			status_bar_errorf("File \"%s\" already exists", buf);
			return 1;
		}
	}

	snprintf(buf, sizeof(buf), "g%c in %s: ", toupper ? 'U' : 'u',
			replace_home_part(view->curr_dir));

	get_group_file_list(view->selected_filelist, view->selected_files, buf);
	cmd_group_begin(buf);
	k = 0;
	for(i = 0; i < n; i++)
	{
		int pos;
		if(strcmp(dest[i], view->selected_filelist[i]) == 0)
			continue;
		pos = find_file_pos_in_list(view, view->selected_filelist[i]);
		if(pos == view->list_pos)
		{
			(void)replace_string(&view->dir_entry[pos].name, dest[i]);
		}
		if(mv_file(view->selected_filelist[i], view->curr_dir, dest[i],
				view->curr_dir, 0) == 0)
			k++;
	}
	cmd_group_end();

	free_selected_file_array(view);
	view->selected_files = 0;
	free_string_array(dest, n);
	status_bar_messagef("%d file%s renamed", k, (k == 1) ? "" : "s");
	return 1;
}

static int
is_copy_list_ok(const char *dst, int count, char **list)
{
	int i;
	for(i = 0; i < count; i++)
	{
		if(path_exists_at(dst, list[i]))
		{
			status_bar_errorf("File \"%s\" already exists", list[i]);
			return 0;
		}
	}
	return 1;
}

/* type:
 *  <= 0 - copy
 *  1 - absolute symbolic links
 *  2 - relative symbolic links
 */
static int
cp_file(const char *src_dir, const char *dst_dir, const char *src,
		const char *dst, int type)
{
	char full_src[PATH_MAX], full_dst[PATH_MAX];
	int op;
	int result;

	snprintf(full_src, sizeof(full_src), "%s/%s", src_dir, src);
	chosp(full_src);
	snprintf(full_dst, sizeof(full_dst), "%s/%s", dst_dir, dst);
	chosp(full_dst);

	if(strcmp(full_src, full_dst) == 0)
		return 0;

	if(type <= 0)
	{
		op = OP_COPY;
	}
	else
	{
		op = OP_SYMLINK;
		if(type == 2)
		{
			snprintf(full_src, sizeof(full_src), "%s", make_rel_path(full_src,
					dst_dir));
		}
	}

	result = perform_operation(op, NULL, full_src, full_dst);
	if(result == 0 && type >= 0)
		add_operation(op, NULL, NULL, full_src, full_dst);
	return result;
}

static int
cpmv_prepare(FileView *view, char ***list, int *nlines, int move, int type,
		int force, char *buf, size_t buf_len, char *path, int *from_file,
		int *from_trash)
{
	int error = 0;

	if(move && !check_if_dir_writable(DR_CURRENT, view->curr_dir))
		return -1;

	if(move == 0 && type == 0 && !have_read_access(view))
		return -1;

	if(*nlines == 1)
	{
		if(is_dir_path(other_view, (*list)[0], path))
			*nlines = 0;
	}
	else
	{
		strcpy(path, other_view->curr_dir);
	}
	if(!check_if_dir_writable(DR_DESTINATION, path))
		return -1;

	get_all_selected_files(view);

	*from_file = *nlines < 0;
	if(*from_file)
	{
		*list = edit_list(view->selected_files, view->selected_filelist, nlines, 1);
		if(*list == NULL)
		{
			return -1;
		}
	}

	if(*nlines > 0 &&
			(!is_name_list_ok(view->selected_files, *nlines, *list, NULL) ||
			(!is_copy_list_ok(path, *nlines, *list) && !force)))
		error = 1;
	if(*nlines == 0 && !force &&
			!is_copy_list_ok(path, view->selected_files, view->selected_filelist))
		error = 1;
	if(error)
	{
		clean_selected_files(view);
		redraw_view(view);
		if(*from_file)
			free_string_array(*list, *nlines);
		return 1;
	}

	if(move)
		strcpy(buf, "move");
	else if(type == 0)
		strcpy(buf, "copy");
	else if(type == 1)
		strcpy(buf, "alink");
	else
		strcpy(buf, "rlink");
	snprintf(buf + strlen(buf), buf_len - strlen(buf), " from %s to ",
			replace_home_part(view->curr_dir));
	snprintf(buf + strlen(buf), buf_len - strlen(buf), "%s: ",
			replace_home_part(path));
	make_undo_string(view, buf, *nlines, *list);

	if(move)
	{
		int i = view->list_pos;
		while(i < view->list_rows - 1 && view->dir_entry[i].selected)
			i++;
		view->list_pos = i;
	}

	*from_trash = path_starts_with(view->curr_dir, cfg.trash_dir);
	return 0;
}

static int
have_read_access(FileView *view)
{
	int i;

#ifdef _WIN32
	if(is_unc_path(view->curr_dir))
		return 1;
#endif

	for(i = 0; i < view->list_rows; i++)
	{
		if(!view->dir_entry[i].selected)
			continue;
		if(access(view->dir_entry[i].name, R_OK) != 0)
		{
			show_error_msgf("Access denied",
					"You don't have read permissions on \"%s\"", view->dir_entry[i].name);
			clean_selected_files(view);
			redraw_view(view);
			return 0;
		}
	}
	return 1;
}

/* Prompts user with a file containing lines from orig array of length count and
 * returns modified list of strings of length *nlines or NULL on error.  The
 * ignore_change parameter makes function think that file is always changed. */
static char **
edit_list(size_t count, char **orig, int *nlines, int ignore_change)
{
	char rename_file[PATH_MAX];
	char **list = NULL;

	generate_tmp_file_name("vifm.rename", rename_file, sizeof(rename_file));

	if(write_file_of_lines(rename_file, orig, count) != 0)
	{
		show_error_msgf("Error Getting List Of Renames",
				"Can't create temporary file \"%s\": %s", rename_file, strerror(errno));
		return NULL;
	}

	if(edit_file(rename_file, ignore_change) > 0)
	{
		list = read_file_of_lines(rename_file, nlines);
		if(list == NULL)
		{
			show_error_msgf("Error Getting List Of Renames",
					"Can't open temporary file \"%s\": %s", rename_file, strerror(errno));
		}
	}

	unlink(rename_file);
	return list;
}

/* Edits the filepath in the editor checking whether it was changed.  Returns
 * negative value on error, zero when no changes were detected and positive
 * number otherwise. */
static int
edit_file(const char filepath[], int force_changed)
{
	struct stat st_before, st_after;

	if(force_changed && stat(filepath, &st_before) != 0)
	{
		show_error_msgf("Error Editing File",
				"Could not stat file \"%s\" before edit: %s", filepath,
				strerror(errno));
		return -1;
	}

	if(view_file(filepath, -1, -1, 0) != 0)
	{
		show_error_msgf("Error Editing File", "Editing of file \"%s\" failed.",
				filepath);
		return -1;
	}

	if(force_changed && stat(filepath, &st_after) != 0)
	{
		show_error_msgf("Error Editing File",
				"Could not stat file \"%s\" after edit: %s", filepath, strerror(errno));
		return -1;
	}

	return force_changed || memcmp(&st_after.st_mtime, &st_before.st_mtime,
			sizeof(st_after.st_mtime)) != 0;
}

int
cpmv_files(FileView *view, char **list, int nlines, int move, int type,
		int force)
{
	int i, processed;
	char buf[COMMAND_GROUP_INFO_LEN + 1];
	char path[PATH_MAX];
	int from_file;
	int from_trash;
	char **sel;
	int sel_len;

	if(!move && type != 0 && !symlinks_available())
	{
		show_error_msg("Symbolic Links Error",
				"Your OS doesn't support symbolic links");
		return 0;
	}

	i = cpmv_prepare(view, &list, &nlines, move, type, force, buf, sizeof(buf),
			path, &from_file, &from_trash);
	if(i != 0)
		return i > 0;

	if(pane_in_dir(curr_view, path) && force)
	{
		show_error_msg("Operation Error",
				"Forcing overwrite when destination and source is same directory will "
				"lead to losing data");
		return 0;
	}

	sel_len = view->selected_files;
	sel = copy_string_array(view->selected_filelist, sel_len);
	if(!view->user_selection)
	{
		/* Clean selection so that it won't get stored for gs command. */
		erase_selection(view);
	}

	processed = 0;
	cmd_group_begin(buf);
	for(i = 0; i < sel_len; i++)
	{
		char dst_full[PATH_MAX];
		const char *dst = (nlines > 0) ? list[i] : sel[i];
		if(from_trash)
		{
			while(isdigit(*dst))
				dst++;
			dst++;
		}

		snprintf(dst_full, sizeof(dst_full), "%s/%s", path, dst);
		if(path_exists(dst_full))
		{
			(void)perform_operation(OP_REMOVESL, NULL, dst_full, NULL);
		}

		if(move)
		{
			progress_msg("Moving files", i + 1, sel_len);

			if(mv_file(sel[i], view->curr_dir, dst, path, 0) != 0)
				view->list_pos = find_file_pos_in_list(view, sel[i]);
			else
				processed++;
		}
		else
		{
			if(type == 0)
				progress_msg("Copying files", i + 1, sel_len);
			if(cp_file(view->curr_dir, path, sel[i], dst, type) == 0)
				processed++;
		}
	}
	cmd_group_end();

	free_string_array(sel, sel_len);
	free_selected_file_array(view);
	clean_selected_files(view);
	load_saving_pos(view, 1);
  load_saving_pos(other_view, 1);
	if(from_file)
		free_string_array(list, nlines);

	status_bar_messagef("%d file%s successfully processed", processed,
			(processed == 1) ? "" : "s");

	return 1;
}

static int
cpmv_files_bg_i(char **list, int nlines, int move, int force, char **sel_list,
		int sel_list_len, int from_trash, const char *src, const char *path)
{
	int i;
	for(i = 0; i < sel_list_len; i++)
	{
		char dst_full[PATH_MAX];
		const char *dst = (nlines > 0) ? list[i] : sel_list[i];
		if(from_trash)
		{
			while(isdigit(*dst))
				dst++;
			dst++;
		}

		snprintf(dst_full, sizeof(dst_full), "%s/%s", path, dst);
		if(path_exists(dst_full))
		{
			perform_operation(OP_REMOVESL, NULL, dst_full, NULL);
		}

		if(move)
			(void)mv_file(sel_list[i], src, dst, path, -1);
		else
			(void)cp_file(src, path, sel_list[i], dst, -1);

		inner_bg_next();
	}
	return 0;
}

static void *
cpmv_stub(void *arg)
{
	bg_args_t *args = (bg_args_t *)arg;

	add_inner_bg_job(args->job);

	cpmv_files_bg_i(args->list, args->nlines, args->move, args->force,
			args->sel_list, args->sel_list_len, args->from_trash, args->src,
			args->path);

	remove_inner_bg_job();

	free_string_array(args->list, args->nlines);
	free_string_array(args->sel_list, args->sel_list_len);
	free(args);
	return NULL;
}

int
cpmv_files_bg(FileView *view, char **list, int nlines, int move, int force)
{
	pthread_t id;
	int i;
	char buf[COMMAND_GROUP_INFO_LEN];
	bg_args_t *args = malloc(sizeof(*args));
	
	args->list = NULL;
	args->nlines = nlines;
	args->move = move;
	args->force = force;

	i = cpmv_prepare(view, &list, &args->nlines, move, 0, force, buf, sizeof(buf),
			args->path, &args->from_file, &args->from_trash);
	if(i != 0)
	{
		free(args);
		return i > 0;
	}

	if(args->from_file)
		args->list = list;
	else
		args->list = copy_string_array(list, nlines);

	args->sel_list = view->selected_filelist;
	args->sel_list_len = view->selected_files;

	view->selected_filelist = NULL;
	free_selected_file_array(view);
	clean_selected_files(view);
	load_saving_pos(view, 1);

	strcpy(args->src, view->curr_dir);

#ifndef _WIN32
	args->job = add_background_job(-1, buf, -1);
#else
	args->job = add_background_job(-1, buf, (HANDLE)-1);
#endif
	if(args->job == NULL)
	{
		free_string_array(args->list, args->nlines);
		free_string_array(args->sel_list, args->sel_list_len);
		free(args);
		return 0;
	}

	args->job->total = args->sel_list_len;
	args->job->done = 0;

	pthread_create(&id, NULL, cpmv_stub, args);
	return 0;
}

static void
go_to_first_file(FileView *view, char **names, int count)
{
	int i;
	load_saving_pos(view, 1);
	for(i = 0; i < view->list_rows; i++)
	{
		char name[PATH_MAX];
		snprintf(name, sizeof(name), "%s", view->dir_entry[i].name);
		chosp(name);
		if(is_in_string_array(names, count, name))
		{
			view->list_pos = i;
			break;
		}
	}
	redraw_view(view);
}

void
make_dirs(FileView *view, char **names, int count, int create_parent)
{
	char buf[COMMAND_GROUP_INFO_LEN + 1];
	int i;
	int n;
	void *cp;

	if(!check_if_dir_writable(DR_CURRENT, view->curr_dir))
	{
		return;
	}

	cp = (void *)(size_t)create_parent;

	for(i = 0; i < count; i++)
	{
		struct stat st;
		if(is_in_string_array(names, i, names[i]))
		{
			status_bar_errorf("Name \"%s\" duplicates", names[i]);
			return;
		}
		if(names[i][0] == '\0')
		{
			status_bar_errorf("Name #%d is empty", i + 1);
			return;
		}
		if(lstat(names[i], &st) == 0)
		{
			status_bar_errorf("File \"%s\" already exists", names[i]);
			return;
		}
	}

	snprintf(buf, sizeof(buf), "mkdir in %s: ",
			replace_home_part(view->curr_dir));

	get_group_file_list(names, count, buf);
	cmd_group_begin(buf);
	n = 0;
	for(i = 0; i < count; i++)
	{
		char full[PATH_MAX];
		snprintf(full, sizeof(full), "%s/%s", view->curr_dir, names[i]);
		if(perform_operation(OP_MKDIR, cp, full, NULL) == 0)
		{
			add_operation(OP_MKDIR, cp, NULL, full, "");
			n++;
		}
		else if(i == 0)
		{
			i--;
			names++;
			count--;
		}
	}
	cmd_group_end();

	if(count > 0)
	{
		if(create_parent)
		{
			for(i = 0; i < count; i++)
			{
				break_at(names[i], '/');
			}
		}
		go_to_first_file(view, names, count);
	}

	if(n == 1)
		status_bar_message("1 directory created");
	else
		status_bar_messagef("%d directories created", n);
}

int
make_files(FileView *view, char **names, int count)
{
	int i;
	int n;
	char buf[COMMAND_GROUP_INFO_LEN + 1];

	if(!check_if_dir_writable(DR_CURRENT, view->curr_dir))
		return 0;

	for(i = 0; i < count; i++)
	{
		struct stat st;
		if(is_in_string_array(names, i, names[i]))
		{
			status_bar_errorf("Name \"%s\" duplicates", names[i]);
			return 1;
		}
		if(names[i][0] == '\0')
		{
			status_bar_errorf("Name #%d is empty", i + 1);
			return 1;
		}
		if(contains_slash(names[i]))
		{
			status_bar_errorf("Name \"%s\" contains slash", names[i]);
			return 1;
		}
		if(lstat(names[i], &st) == 0)
		{
			status_bar_errorf("File \"%s\" already exists", names[i]);
			return 1;
		}
	}

	snprintf(buf, sizeof(buf), "touch in %s: ",
			replace_home_part(view->curr_dir));

	get_group_file_list(names, count, buf);
	cmd_group_begin(buf);
	n = 0;
	for(i = 0; i < count; i++)
	{
		char full[PATH_MAX];
		snprintf(full, sizeof(full), "%s/%s", view->curr_dir, names[i]);
		if(perform_operation(OP_MKFILE, NULL, full, NULL) == 0)
		{
			add_operation(OP_MKFILE, NULL, NULL, full, "");
			n++;
		}
	}
	cmd_group_end();

	if(n > 0)
		go_to_first_file(view, names, count);

	status_bar_messagef("%d file%s created", n, (n == 1) ? "" : "s");
	return 1;
}

int
check_if_dir_writable(DirRole dir_role, const char *path)
{
	if(is_dir_writable(path))
		return 1;

	if(dir_role == DR_DESTINATION)
		show_error_msg("Operation error", "Destination directory is not writable");
	else
		show_error_msg("Operation error", "Current directory is not writable");
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
