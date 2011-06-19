/* vifm
 * Copyright (C) 2001 Ken Steen.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <ncurses.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h> /* stat */
#include <sys/types.h> /* waitpid() */
#include <sys/wait.h> /* waitpid() */
#include <unistd.h>

#include <errno.h> /* errno */
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h> /* swprintf */

#include "background.h"
#include "cmdline.h"
#include "color_scheme.h"
#include "commands.h"
#include "config.h"
#include "filelist.h"
#include "fileops.h"
#include "filetype.h"
#include "menus.h"
#include "registers.h"
#include "status.h"
#include "ui.h"
#include "utils.h"

static char rename_file_ext[NAME_MAX];

static struct {
	registers_t *reg;
	FileView *view;
	int force_move;
	int x, y;
	const char *name;
} put_confirm;

static void put_confirm_cb(const char *dest_name);
static void put_decide_cb(const char *dest_name);
static int put_files_from_register_i(FileView *view);

int
my_system(char *command)
{
	int pid;
	int status;
	extern char **environ;

	if(command == NULL)
		return 1;

	pid = fork();
	if(pid == -1)
		return -1;
	if(pid == 0)
	{
		char *args[4];

		signal(SIGINT, SIG_DFL);

		args[0] = "sh";
		args[1] = "-c";
		args[2] = command;
		args[3] = 0;
		execve("/bin/sh", args, environ);
		exit(127);
	}
	do
	{
		if(waitpid(pid, &status, 0) == -1)
		{
			if(errno != EINTR)
				return -1;
		}
		else
			return status;
	}while(1);
}

void
unmount_fuse(void)
{
	Fuse_List *runner = fuse_mounts;
	char buf[8192];

	chdir("/");
	while(runner)
	{
		char *tmp = escape_filename(runner->mount_point, 0, 1);
		snprintf(buf, sizeof(buf), "sh -c \"fusermount -u %s\"", tmp);
		free(tmp);
		my_system(buf);
		if(access(runner->mount_point, F_OK) == 0)
			rmdir(runner->mount_point);
		runner = runner->next;
	}

	leave_invalid_dir(&lwin, lwin.curr_dir);
	leave_invalid_dir(&rwin, rwin.curr_dir);
}

int
system_and_wait_for_errors(char *cmd)
{
	pid_t pid;
	int error_pipe[2];
	int result = 0;

	if(pipe(error_pipe) != 0)
	{
		show_error_msg("File pipe error", "Error creating pipe");
		return -1;
	}

	if((pid = fork()) == -1)
		return -1;

	if(pid == 0)
	{
		run_from_fork(error_pipe, 1, cmd);
	}
	else
	{
		char buf[80*10];
		char linebuf[80];
		int nread = 0;

		close(error_pipe[1]); /* Close write end of pipe. */

		buf[0] = '\0';
		while((nread = read(error_pipe[0], linebuf, sizeof(linebuf) - 1)) > 0)
		{
			result = -1;
			linebuf[nread + 1] = '\0';
			if(nread == 1 && linebuf[0] == '\n')
				continue;
			strcat(buf, linebuf);
		}
		close(error_pipe[0]);

		if(result != 0)
			show_error_msg("Background Process Error", buf);
	}

	return result;
}

static int
execute(char **args)
{
	int pid;

	if((pid = fork()) == 0)
	{
		/* Run as a separate session */
		setsid();
		close(0);
		execvp(args[0], args);
		exit(127);
	}

	return pid;
}

/* returns new value for save_msg */
int
yank_files(FileView *view, int reg, int count, int *indexes)
{
	char buf[32];
	int tmp;
	if(count > 0)
		get_selected_files(view, count, indexes);
	else
		get_all_selected_files(curr_view);

	yank_selected_files(curr_view, reg);
	tmp = curr_view->selected_files;
	free_selected_file_array(curr_view);

	if(count == 0)
	{
		clean_selected_files(curr_view);
		draw_dir_list(curr_view, curr_view->top_line, curr_view->list_pos);
		moveto_list_pos(curr_view, curr_view->list_pos);
		count = tmp;
	}

	snprintf(buf, sizeof(buf), " %d %s yanked.", count,
			count == 1 ? "file" : "files");
	status_bar_message(buf);

	return count;
}

void
yank_selected_files(FileView *view, int reg)
{
	int x;
	size_t namelen;

	/* A - Z  append to register otherwise replace */
	if(reg >= 'A' && reg <= 'Z')
		reg += 'a' - 'A';
	else
		clear_register(reg);

	for(x = 0; x < view->selected_files; x++)
	{
		if(!view->selected_filelist[x])
			break;

		char buf[PATH_MAX];
		namelen = strlen(view->selected_filelist[x]);
		snprintf(buf, sizeof(buf), "%s/%s", view->curr_dir,
				view->selected_filelist[x]);
		append_to_register(reg, buf);
	}
}

/* execute command. */
int
file_exec(char *command)
{
	char *args[4];
	pid_t pid;

	args[0] = "sh";
	args[1] = "-c";
	args[2] = command;
	args[3] = NULL;

	pid = execute(args);
	return pid;
}

void
view_file(FileView *view)
{
	char command[PATH_MAX + 5] = "";
	char *filename = escape_filename(get_current_file_name(view),
			strlen(get_current_file_name(view)), 0);

	snprintf(command, sizeof(command), "%s %s", cfg.vi_command, filename);

	shellout(command, 0);
	free(filename);
	curs_set(0);
}

int
check_link_is_dir(FileView *view, int pos)
{
	char linkto[PATH_MAX + NAME_MAX];
	int len;
	char *filename = strdup(view->dir_entry[pos].name);
	len = strlen(filename);
	if (filename[len - 1] == '/')
		filename[len - 1] = '\0';

	len = readlink (filename, linkto, sizeof (linkto));

	free(filename);

	if (len > 0)
	{
		struct stat s;
		linkto[len] = '\0';
		if(lstat(linkto, &s) != 0)
			return 0;

		if((s.st_mode & S_IFMT) == S_IFDIR)
			return 1;
	}

	return 0;
}

static void
get_last_path_component(const char *path, char* buf)
{
	const char *p, *q;

	p = path + strlen(path) - 1;
	if(*p == '/')
		p--;

	q = p;
	while(p - 1 >= path && *(p - 1) != '/')
		p--;

	snprintf(buf, q - p + 1 + 1, "%s", p);
}

void
cd_updir(FileView *view)
{
	char dir_name[NAME_MAX + 1] = "";

	get_last_path_component(view->curr_dir, dir_name);
	strcat(dir_name, "/");

	if(change_directory(view, "../") != 1)
	{
		load_dir_list(view, 0);
		moveto_list_pos(view, find_file_pos_in_list(view, dir_name));
	}
}

/* mount_point should be an array of at least PATH_MAX characters */
static void
fuse_mount(FileView *view, char *filename, char *program, char *mount_point)
{
	Fuse_List *runner;
	int mount_point_id = 0;
	Fuse_List *fuse_item = NULL;
	char buf[2*PATH_MAX];
	char cmd_buf[96];
	char *cmd_pos;
	char *buf_pos;
	char *prog_pos;
	int clear_before_mount = 0;

	/* get mount_point_id + mount_point and set runner pointing to the list's
	 * tail */
	if(fuse_mounts)
	{
		runner = fuse_mounts;
		while(runner->next != NULL)
			runner = runner->next;
		mount_point_id = runner->mount_point_id + 1;
	}
	snprintf(mount_point, PATH_MAX, "%s/%03d_%s", cfg.fuse_home, mount_point_id,
			get_current_file_name(view));
	if(mkdir(mount_point, S_IRWXU))
	{
		show_error_msg("Unable to create FUSE mount directory", mount_point);
		return;
	}
	/* I need the return code of the mount command */
	status_bar_message("FUSE mounting selected file, please stand by..");
	buf_pos = buf;
	prog_pos = program;
	/* Build the mount command based on the FUSE program config line in vifmrc.
		 Accepted FORMAT: FUSE_MOUNT|some_mount_command %SOURCE_FILE %DESTINATION_DIR
		 or
		 FUSE_MOUNT2|some_mount_command %PARAM %DESTINATION_DIR */
	strcpy(buf_pos, "sh -c \"");
	/* TODO what is this?
		 strcpy(buf_pos, "sh -c \"pauseme PAUSE_ON_ERROR_ONLY "); */
	buf_pos += strlen(buf_pos);
	while(*prog_pos != '\0' && *prog_pos != '|')
		prog_pos++;
	prog_pos++;
	while(*prog_pos != '\0')
	{
		if(*prog_pos == '%')
		{
			cmd_pos = cmd_buf;
			while(*prog_pos != '\0' && *prog_pos != ' ')
			{
				*cmd_pos = *prog_pos;
				if(cmd_pos < cmd_pos+sizeof(cmd_buf))
					cmd_pos++;
				prog_pos++;
			}
			*cmd_pos = '\0';
			if(buf_pos + strlen(filename) >= buf + sizeof(buf) + 2)
				continue;
			else if(!strcmp(cmd_buf, "%SOURCE_FILE") || !strcmp(cmd_buf, "%PARAM"))
			{
				*buf_pos++ = '\'';
				strcpy(buf_pos, filename);
				buf_pos += strlen(filename);
				*buf_pos++ = '\'';
			}
			else if(!strcmp(cmd_buf, "%DESTINATION_DIR"))
			{
				*buf_pos++ = '\'';
				strcpy(buf_pos, mount_point);
				buf_pos += strlen(mount_point);
				*buf_pos++ = '\'';
			}
			else if(!strcmp(cmd_buf, "%CLEAR"))
			{
				clear_before_mount = 1;
			}
		}
		else
		{
			*buf_pos = *prog_pos;
			if(buf_pos < buf_pos + sizeof(buf) - 1)
				buf_pos++;
			prog_pos++;
		}
	}

	*buf_pos = '"';
	*(++buf_pos) = '\0';
	/* CMD built */
	/* Just before running the mount,
		 I need to chdir out temporarily from any FUSE mounted
		 paths, Otherwise the fuse-zip command fails with
		 "fusermount: failed to open current directory: permission denied"
		 (this happens when mounting JARs from mounted JARs) */
	chdir(cfg.fuse_home);
	/*
		 TODO see what's here
		 def_prog_mode();
		 endwin();
		 my_system("clear");
		 int status = my_system(buf);
		 */

	if(clear_before_mount)
	{
		def_prog_mode();
		endwin();
		my_system("clear");
	}

	int status = background_and_wait_for_status(buf);
	/* check child status */
	if(!WIFEXITED(status) || (WIFEXITED(status) && WEXITSTATUS(status)))
	{
		werase(status_bar);
		/* remove the DIR we created for the mount */
		if(!access(mount_point, F_OK))
			rmdir(mount_point);
		show_error_msg("FUSE MOUNT ERROR", filename);
		chdir(view->curr_dir);
		return;
	}
	status_bar_message("FUSE mount success.");

	fuse_item = (Fuse_List *)malloc(sizeof(Fuse_List));
	strcpy(fuse_item->source_file_name, filename);
	strcpy(fuse_item->source_file_dir, view->curr_dir);
	strcpy(fuse_item->mount_point, mount_point);
	fuse_item->mount_point_id = mount_point_id;
	fuse_item->next = NULL;
	if(!fuse_mounts)
		fuse_mounts = fuse_item;
	else
		runner->next = fuse_item;
}

/* wont mount same file twice */
static void
fuse_try_mount(FileView *view, char *program)
{
	Fuse_List *runner;
	char filename[PATH_MAX];
	char mount_point[PATH_MAX];
	int mount_found;

	if(access(cfg.fuse_home, F_OK|W_OK|X_OK) != 0)
	{
		if(mkdir(cfg.fuse_home, S_IRWXU))
		{
			show_error_msg("Unable to create FUSE mount home directory",
					cfg.fuse_home);
			return;
		}
	}

	runner = fuse_mounts;
	mount_found = 0;

	snprintf(filename, PATH_MAX, "%s/%s", view->curr_dir,
			get_current_file_name(view));
	/* check if already mounted */
	while(runner)
	{
		if(!strcmp(filename, runner->source_file_name))
		{
			strcpy(mount_point, runner->mount_point);
			mount_found = 1;
			break;
		}
		runner = runner->next;
	}

	if(!mount_found)
	{
		/* new file to be mounted */
		if(strncmp(program, "FUSE_MOUNT2", 11) == 0)
		{
			FILE *f;
			size_t len;
			if((f = fopen(filename, "r")) == NULL)
			{
				status_bar_message("SSH mount failed: can't open file for reading");
				curr_stats.save_msg = 1;
				return;
			}

			fgets(filename, sizeof(filename), f);
			len = strlen(filename);

			if(len == 0 || (len == 1 && filename[0] == '\n'))
			{
				status_bar_message("SSH mount failed: file is empty");
				curr_stats.save_msg = 1;
				return;
			}

			if(filename[len - 1] == '\n')
				filename[len - 1] = '\0';

			fclose(f);
		}
		fuse_mount(view, filename, program, mount_point);
	}

	change_directory(view, mount_point);
	load_dir_list(view, 0);
	moveto_list_pos(view, view->curr_line);
}

static void
execute_file(FileView *view, int dont_execute)
{
	char *program;

	/* Check for a filetype */
	/* vi is set as the default for any extension without a program */
	if((program = get_default_program_for_file(
			view->dir_entry[view->list_pos].name)) == NULL)
	{
		view_file(view);
		return;
	}

	if(strncmp(program, "FUSE_MOUNT", 10) == 0
			|| strncmp(program, "FUSE_MOUNT2", 11) == 0)
	{
		if(dont_execute)
			view_file(view);
		else
			fuse_try_mount(view, program);
	}
	else if(strchr(program, '%') != NULL)
	{
		int m, s;
		char *command = expand_macros(view, program, NULL, &m, &s);
		shellout(command, 0);
		free(command);
	}
	else
	{
		char buf[NAME_MAX *2];
		char *temp = escape_filename(view->dir_entry[view->list_pos].name,
				strlen(view->dir_entry[view->list_pos].name), 0);

		snprintf(buf, sizeof(buf), "%s %s", program, temp);
		shellout(buf, 0);
		free(temp);
	}
	free(program);
}

static void
follow_link(FileView *view)
{
	struct stat s;
	int is_dir = 0, is_file = 0;
	char *dir = NULL, *file = NULL, *link_dup;
	char linkto[PATH_MAX + NAME_MAX];
	int len;
	char *filename;

	filename = strdup(view->dir_entry[view->list_pos].name);
	len = strlen(filename);
	if(filename[len - 1] == '/')
		filename[len - 1] = '\0';

	len = readlink (filename, linkto, sizeof (linkto));

	free(filename);

	if(len == 0)
	{
		status_bar_message("Couldn't Resolve Link");
		return;
	}

	linkto[len] = '\0';
	link_dup = strdup(linkto);

	lstat(linkto, &s);

	if((s.st_mode & S_IFMT) == S_IFDIR)
	{
		is_dir = 1;
		dir = strdup(view->dir_entry[view->list_pos].name);
	}
	else
	{
		int x;
		for(x = strlen(linkto); x > 0; x--)
		{
			if(linkto[x] == '/')
			{
				linkto[x] = '\0';
				lstat(linkto, &s);
				if((s.st_mode & S_IFMT) == S_IFDIR)
				{
					is_dir = 1;
					dir = strdup(linkto);
					break;
				}
			}
		}
		if((file = strrchr(link_dup, '/')) != NULL)
		{
			file++;
			is_file = 1;
		}
	}
	if(is_dir)
	{
		change_directory(view, dir);
		load_dir_list(view, 0);
		moveto_list_pos(view, view->curr_line);

		if(is_file)
		{
			int pos = find_file_pos_in_list(view, file);
			if(pos >= 0)
				moveto_list_pos(view, pos);
		}
	}
	else
	{
		int pos = find_file_pos_in_list(view, link_dup);
		if(pos >= 0)
			moveto_list_pos(view, pos);
	}
	free(link_dup);
	free(dir);
}

void
handle_file(FileView *view, int dont_execute)
{
	int type;

	type = view->dir_entry[view->list_pos].type;

	if(type == DIRECTORY)
	{
		char *filename = get_current_file_name(view);

		if(strcmp(filename, "../") == 0)
			cd_updir(view);
		else if(change_directory(view, filename) == 0)
		{
			load_dir_list(view, 0);
			moveto_list_pos(view, view->curr_line);
		}
		return;
	}

	if(cfg.vim_filter)
	{
		FILE *fp;
		char filename[PATH_MAX] = "";

		snprintf(filename, sizeof(filename), "%s/vimfiles", cfg.config_dir);
		fp = fopen(filename, "w");
		snprintf(filename, sizeof(filename), "%s/%s", view->curr_dir,
				view->dir_entry[view->list_pos].name);
		endwin();
		fprintf(fp, "%s", filename);
		fclose(fp);
		exit(0);
	}

	if(type == EXECUTABLE && !dont_execute && cfg.auto_execute)
	{
		char buf[NAME_MAX];
		snprintf(buf, sizeof(buf), "./%s", get_current_file_name(view));
		shellout(buf, 1);
	}
	else if(type == REGULAR || type == EXECUTABLE)
	{
		execute_file(view, dont_execute);
	}
	else if(type == LINK)
	{
		follow_link(view);
	}
}

int
pipe_and_capture_errors(char *command)
{
  int file_pipes[2];
  int pid;
	int nread;
	int error = 0;
  char *args[4];

  if(pipe(file_pipes) != 0)
	  return 1;

  if((pid = fork()) == -1)
	  return 1;

  if(pid == 0)
	{
		close(1);
		close(2);
		dup(file_pipes[1]);
		close(file_pipes[0]);
		close(file_pipes[1]);

		args[0] = "sh";
		args[1] = "-c";
		args[2] = command;
		args[3] = NULL;
		execvp(args[0], args);
		exit(127);
	}
	else
	{
		char buf[1024];
		close (file_pipes[1]);
		while((nread = read(*file_pipes, buf, sizeof(buf) -1)) > 0)
		{
			buf[nread] = '\0';
			error = nread;
		}
		if(error > 1)
		{
			char title[strlen(command) +4];
			snprintf(title, sizeof(title), " %s ", command);
			show_error_msg(title, buf);
			return 1;
		}
	}
	return 0;
}

static void
progress_msg(const char *text, int ready, int total)
{
	char msg[strlen(text) + 32];

	sprintf(msg, "%s %d/%d", text, ready, total);
	show_progress(msg, 1);
}

/* returns new value for save_msg */
int
delete_file(FileView *view, int reg, int count, int *indexes, int use_trash)
{
	char buf[256];
	int x, y;

	if(count > 0)
		get_selected_files(view, count, indexes);
	else
	{
		if(view->selected_files == 0)
		{
			view->dir_entry[view->list_pos].selected = 1;
			view->selected_files = 1;
		}
		get_all_selected_files(view);
	}

	/* A - Z  append to register otherwise replace */
	if(reg >= 'A' && reg <= 'Z')
		reg += 'a' - 'A';
	else
		clear_register(reg);

	y = 0;
	chdir(curr_view->curr_dir);
	for(x = 0; x < view->selected_files; x++)
	{
		char *esc_file;
		if(strcmp("../", view->selected_filelist[x]) == 0)
		{
			show_error_msg("Background Process Error",
					"You cannot delete the ../ directory");
			continue;
		}

		esc_file = escape_filename(view->selected_filelist[x], 0, 1);
		if(cfg.use_trash && use_trash)
		{
			snprintf(buf, sizeof(buf), "mv %s '%s'", esc_file, cfg.trash_dir);
		}
		else
			snprintf(buf, sizeof(buf), "rm -rf %s", esc_file);
		free(esc_file);

		progress_msg("Deleting files", x + 1, view->selected_files);
		if(background_and_wait_for_errors(buf) == 0)
		{
			char reg_buf[PATH_MAX];
			snprintf(reg_buf, sizeof(reg_buf), "%s/%s", cfg.trash_dir,
					view->selected_filelist[x]);
			append_to_register(reg, reg_buf);
			y++;
		}
	}
	free_selected_file_array(view);

	get_all_selected_files(view);
	load_dir_list(view, 1);
	free_selected_file_array(view);

	/* some files may still exist if there was an error */
	for(x = 0; x < view->list_rows; x++)
	{
		view->selected_files += view->dir_entry[x].selected;
	}

	moveto_list_pos(view, view->list_pos);

	snprintf(buf, sizeof(buf), "%d %s Deleted", y, y == 1 ? "File" : "Files");
	status_bar_message(buf);
	return 1;
}

static void
rename_file_cb(const char *new_name)
{
	char *filename = get_current_file_name(curr_view);
	char *escaped_src, *escaped_dst;
	char command[1024];
	char new[NAME_MAX + 1];
	size_t len;
	int found;

	len = strlen(filename);
	snprintf(new, sizeof(new), "%s%s%s", new_name,
			filename[len - 1] == '/' ? "/" : "", rename_file_ext);

	/* Filename unchanged */
	if(strcmp(filename, new) == 0)
		return;

	if(access(new, F_OK) == 0 && strncmp(filename, new, strlen(filename)) != 0)
	{
		show_error_msg("File exists",
				"That file already exists. Will not overwrite.");

		load_dir_list(curr_view, 1);
		moveto_list_pos(curr_view, curr_view->list_pos);
		return;
	}

	escaped_src = escape_filename(filename, strlen(filename), 0);
	escaped_dst = escape_filename(new, strlen(new), 0);
	if(escaped_src == NULL || escaped_dst == NULL)
	{
		free(escaped_src);
		free(escaped_dst);
		return;
	}
	snprintf(command, sizeof(command), "mv -f %s %s", escaped_src, escaped_dst);
	free(escaped_src);
	free(escaped_dst);

	if(system_and_wait_for_errors(command) != 0)
	{
		load_dir_list(curr_view, 1);
		moveto_list_pos(curr_view, curr_view->list_pos);
		return;
	}

	load_dir_list(curr_view, 0);
	found = find_file_pos_in_list(curr_view, new);
	if(found >= 0)
		moveto_list_pos(curr_view, found);
	else
		moveto_list_pos(curr_view, curr_view->list_pos);
}

void
rename_file(FileView *view, int name_only)
{
	size_t len;
	char* p;
	char buf[NAME_MAX + 1];

	strncpy(buf, get_current_file_name(curr_view), sizeof(buf));
	buf[sizeof(buf) - 1] = '\0';
	if(strcmp(buf, "../") == 0)
	{
		curr_stats.save_msg = 1;
		status_bar_message("You can't rename parent directory this way.");
		return;
	}

	len = strlen(buf);
	if(buf[len - 1] == '/')
		buf[len - 1] = '\0';

	if (!name_only || (p = strchr(buf, '.')) == NULL) {
		rename_file_ext[0] = '\0';
	} else {
		strcpy(rename_file_ext, p);
		*p = '\0';
	}

	enter_prompt_mode(L"New name: ", buf, rename_file_cb);
}

static void
change_owner_cb(const char *new_owner)
{
	char *filename;
	char command[1024];
	char *escaped;

	filename = get_current_file_name(curr_view);
	escaped = escape_filename(filename, strlen(filename), 0);
	snprintf(command, sizeof(command), "chown -fR %s %s", new_owner, escaped);
	free(escaped);

	if(system_and_wait_for_errors(command) != 0)
		return;

	load_dir_list(curr_view, 1);
	moveto_list_pos(curr_view, curr_view->list_pos);
}

void
change_owner(FileView *view)
{
	enter_prompt_mode(L"New owner: ", "", change_owner_cb);
}

static void
change_group_cb(const char *new_owner)
{
	char *filename;
	char command[1024];
	char *escaped;

	filename = get_current_file_name(curr_view);
	escaped = escape_filename(filename, strlen(filename), 0);
	snprintf(command, sizeof(command), "chown -fR :%s %s", new_owner, escaped);
	free(escaped);

	if(system_and_wait_for_errors(command) != 0)
		return;

	load_dir_list(curr_view, 1);
	moveto_list_pos(curr_view, curr_view->list_pos);
}

void
change_group(FileView *view)
{
	enter_prompt_mode(L"New group: ", "", change_group_cb);
}

static void
prompt_dest_name(const char *src_name)
{
	wchar_t buf[256];

	swprintf(buf, sizeof(buf)/sizeof(buf[0]), L"New name for %s: ", src_name);
	enter_prompt_mode(buf, src_name, put_confirm_cb);
}

static void
prompt_what_to_do(const char *src_name)
{
	wchar_t buf[256];

	put_confirm.name = src_name;
	swprintf(buf, sizeof(buf)/sizeof(buf[0]),
			L"Name conflict for %s. s(skip)/r(rename)/o(override)/(cancel): ",
			src_name);
	enter_prompt_mode(buf, "r", put_decide_cb);
}

/* Returns 0 on success */
static int
put_next_file(const char *dest_name, int override)
{
	char buf[PATH_MAX + NAME_MAX*2 + 4];
	char *src_buf, *dst_buf;

	snprintf(buf, sizeof(buf), "%s", reg->files[put_confirm.x]);
	chosp(buf);
	src_buf = escape_filename(buf, 0, 1);
	dst_buf = escape_filename(put_confirm.view->curr_dir, 0, 1);
	if(access(buf, F_OK) == 0 && src_buf != NULL && dst_buf != NULL)
	{
		const char *p = dest_name;
		int move = strcmp(buf, cfg.trash_dir) == 0 || put_confirm.force_move;

		if(p[0] == '\0')
			p = strrchr(buf, '/') + 1;

		if(access(p, F_OK) == 0 && !override)
		{
			free(src_buf);
			free(dst_buf);
			prompt_what_to_do(p);
			return 1;
		}
		else if(override)
		{
			put_confirm.view->dir_mtime = 0;
		}

		if(move)
			snprintf(buf, sizeof(buf), "mv %s %s/%s", src_buf, dst_buf, dest_name);
		else
			snprintf(buf, sizeof(buf), "cp -pR %s %s/%s", src_buf, dst_buf, dest_name);

		progress_msg("Putting files", put_confirm.x + 1, reg->num_files);
		if(background_and_wait_for_errors(buf) == 0)
		{
			put_confirm.y++;
			if(move)
			{
				free(reg->files[put_confirm.x]);
				reg->files[put_confirm.x] = NULL;
			}
		}
	}
	free(src_buf);
	free(dst_buf);
	return 0;
}

static void
put_confirm_cb(const char *dest_name)
{
	if(put_next_file(dest_name, 0) == 0)
	{
		put_confirm.x++;
		curr_stats.save_msg = put_files_from_register_i(put_confirm.view);
	}
}

static void
put_decide_cb(const char *choice)
{
	if(strcmp(choice, "s") == 0)
	{
		put_confirm.x++;
		curr_stats.save_msg = put_files_from_register_i(put_confirm.view);
	}
	else if(strcmp(choice, "r") == 0)
	{
		prompt_dest_name(put_confirm.name);
	}
	else if(strcmp(choice, "o") == 0)
	{
		if(put_next_file("", 1) == 0)
		{
			put_confirm.x++;
			curr_stats.save_msg = put_files_from_register_i(put_confirm.view);
		}
	}
	else
	{
		prompt_what_to_do(put_confirm.name);
	}
}

/* Returns new value for save_msg flag. */
static int
put_files_from_register_i(FileView *view)
{
	char buf[PATH_MAX + NAME_MAX*2 + 4];

	chdir(view->curr_dir);
	while(put_confirm.x < put_confirm.reg->num_files)
	{
		if(put_next_file("", 0) != 0)
			return 0;
		put_confirm.x++;
	}

	pack_register(put_confirm.reg->name);

	if(put_confirm.y <= 0)
		return 0;

	snprintf(buf, sizeof(buf), " %d file%s inserted", put_confirm.y,
			(put_confirm.y == 1) ? "" : "s");
	status_bar_message(buf);

	return 1;
}

/* Returns new value for save_msg flag. */
int
put_files_from_register(FileView *view, int name, int force_move)
{
	registers_t *reg;

	reg = find_register(name);

	if(reg == NULL || reg->num_files < 1)
	{
		status_bar_message("Register is empty");
		wrefresh(status_bar);
		return 1;
	}

	put_confirm.reg = reg;
	put_confirm.force_move = force_move;
	put_confirm.x = 0;
	put_confirm.y = 0;
	put_confirm.view = view;
	return put_files_from_register_i(view);
}

void
clone_file(FileView* view)
{
	char buf[PATH_MAX + NAME_MAX*2 + 4];
	char *escaped;
	const char *filename;
	
	filename = get_current_file_name(view);
	if(strcmp(filename, ".") == 0)
		return;
	if(strcmp(filename, "..") == 0)
		return;

	if(view->dir_entry[view->list_pos].type == DIRECTORY)
		escaped = escape_filename(filename, strlen(filename) - 1, 1);
	else
		escaped = escape_filename(filename, strlen(filename), 1);
	snprintf(buf, sizeof(buf), "cp -pR %s %s_clone", escaped, escaped);
	background_and_wait_for_errors(buf);
	free(escaped);
}

unsigned long long
calc_dirsize(const char *path)
{
	DIR* dir;
	struct dirent* dentry;
	const char* slash = "";
	size_t size;

	dir = opendir(path);
	if(dir == NULL)
		return (0);

	if(path[strlen(path) - 1] != '/')
		slash = "/";

	size = 0;
	while((dentry = readdir(dir)) != NULL)
	{
		char buf[PATH_MAX];

		if(strcmp(dentry->d_name, ".") == 0)
			continue;
		else if(strcmp(dentry->d_name, "..") == 0)
			continue;

		snprintf(buf, sizeof (buf), "%s%s%s", path, slash, dentry->d_name);
		if(dentry->d_type == DT_DIR)
		{
			unsigned long long dir_size = (unsigned long long)tree_get_data(
					curr_stats.dirsize_cache, buf);
			if(dir_size == 0)
				dir_size = calc_dirsize(buf);
			size += dir_size;
		}
		else
		{
			struct stat st;
			if(stat(buf, &st) == 0)
				size += st.st_size;
		}
	}

	closedir(dir);

	tree_set_data(curr_stats.dirsize_cache, path, (void*)size);
	return (size);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
