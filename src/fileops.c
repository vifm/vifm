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

#define _GNU_SOURCE

#include <regex.h>

#include <curses.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h> /* stat */
#include <sys/types.h> /* waitpid() */
#ifndef _WIN32
#include <sys/wait.h> /* waitpid() */
#endif
#include <unistd.h>

#include <ctype.h> /* isdigit */
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
#include "ops.h"
#include "registers.h"
#include "status.h"
#include "ui.h"
#include "undo.h"
#include "utils.h"

static char rename_file_ext[NAME_MAX];

static struct {
	registers_t *reg;
	FileView *view;
	int force_move;
	int x, y;
	char *name;
	int overwrite_all;
	int link; /* 0 - no, 1 - absolute, 2 - relative */
} put_confirm;

static void put_confirm_cb(const char *dest_name);
static void put_decide_cb(const char *dest_name);
static int put_files_from_register_i(FileView *view, int start);

int
my_system(char *command)
{
#ifndef _WIN32
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

		args[0] = cfg.shell;
		args[1] = "-c";
		args[2] = command;
		args[3] = NULL;
		execve(cfg.shell, args, environ);
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
#else
	char buf[strlen(cfg.shell) + 5 + strlen(command) + 1 + 1];

	signal(SIGINT, SIG_DFL);

	strcpy(buf, cfg.shell);
	if(strcmp(cfg.shell, "cmd") == 0)
		strcat(buf, " /C \"");
	else
		strcat(buf, " -c \"");
	strcat(buf, command);
	strcat(buf, "\"");

	system("cls");
	return system(buf);
#endif
}

void
unmount_fuse(void)
{
	Fuse_List *runner;

	if(fuse_mounts == NULL)
		return;

	if(chdir("/") != 0)
		return;

	runner = fuse_mounts;
	while(runner)
	{
		char buf[14 + PATH_MAX + 1];
		char *tmp;

		tmp = escape_filename(runner->mount_point, 0);
		snprintf(buf, sizeof(buf), "fusermount -u %s", tmp);
		free(tmp);

		my_system(buf);
		if(access(runner->mount_point, F_OK) == 0)
			rmdir(runner->mount_point);

		runner = runner->next;
	}

	leave_invalid_dir(&lwin, lwin.curr_dir);
	leave_invalid_dir(&rwin, rwin.curr_dir);
}

static int
execute(char **args)
{
#ifndef _WIN32
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
#else
	return -1;
#endif
}

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
		draw_dir_list(view, view->top_line);
		move_to_list_pos(view, view->list_pos);
	}

	status_bar_messagef("%d %s yanked", yanked, yanked == 1 ? "file" : "files");

	return yanked;
}

void
yank_selected_files(FileView *view, int reg)
{
	int x;

	/* A - Z  append to register otherwise replace */
	if(reg >= 'A' && reg <= 'Z')
		reg += 'a' - 'A';
	else
		clear_register(reg);

	for(x = 0; x < view->selected_files; x++)
	{
		char buf[PATH_MAX];
		if(!view->selected_filelist[x])
			break;

		snprintf(buf, sizeof(buf), "%s/%s", view->curr_dir,
				view->selected_filelist[x]);
		chosp(buf);
		append_to_register(reg, buf);
	}
	update_unnamed_reg(reg);
}

/* execute command. */
int
file_exec(char *command)
{
	char *args[4];
	pid_t pid;

	args[0] = cfg.shell;
	args[1] = "-c";
	args[2] = command;
	args[3] = NULL;

	pid = execute(args);
	return pid;
}

/* negative line means ignore it */
void
view_file(const char *filename, int line)
{
	char command[PATH_MAX + 5] = "";
	char *escaped;
	int bg;

	if(access(filename, F_OK) != 0)
	{
		(void)show_error_msg("Broken Link", "Link destination doesn't exist");
		return;
	}

	escaped = escape_filename(filename, 0);
	if(line < 0)
		snprintf(command, sizeof(command), "%s %s", get_vicmd(&bg), escaped);
	else
		snprintf(command, sizeof(command), "%s +%d %s", get_vicmd(&bg), line,
				escaped);
	free(escaped);

	if(bg)
		start_background_job(command);
	else
		shellout(command, -1);
	curs_set(FALSE);
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
		move_to_list_pos(view, find_file_pos_in_list(view, dir_name));
	}
}

/* Returns pointer to a statically allocated buffer */
static const char *
make_name_unique(const char *filename)
{
	static char unique[PATH_MAX];
	size_t len;
	int i;

#ifndef _WIN32
	len = snprintf(unique, sizeof(unique), "%s_%u%u_00", filename, getppid(),
			getpid());
#else
	len = snprintf(unique, sizeof(unique), "%s_%u%u_00", filename, 0, 0);
#endif
	i = 0;

	while(access(unique, F_OK) == 0)
		sprintf(unique + len - 2, "%d", ++i);
	return unique;
}

/*
 * mount_point should be an array of at least PATH_MAX characters
 * Returns 0 on success.
 */
static Fuse_List *
fuse_mount(FileView *view, char *filename, const char *program,
		char *mount_point)
{
	Fuse_List *runner;
	int mount_point_id = 0;
	Fuse_List *fuse_item = NULL;
	char buf[2*PATH_MAX];
	char cmd_buf[96];
	char *cmd_pos;
	char *buf_pos;
	const char *prog_pos;
	char *escaped_path;
	char *escaped_filename;
	char *escaped_mount_point;
	int clear_before_mount = 0;
	const char *tmp_file;
	int status;

	escaped_filename = escape_filename(get_current_file_name(view), 0);

	/* get mount_point_id + mount_point and set runner pointing to the list's
	 * tail */
	if(fuse_mounts)
	{
		runner = fuse_mounts;
		while(runner->next != NULL)
			runner = runner->next;
		mount_point_id = runner->mount_point_id;
	}
	do
	{
		snprintf(mount_point, PATH_MAX, "%s/%03d_%s", cfg.fuse_home,
				++mount_point_id, get_current_file_name(view));
	} while(access(mount_point, F_OK) == 0);
#ifndef _WIN32
	if(mkdir(mount_point, S_IRWXU))
#else
	if(mkdir(mount_point))
#endif
	{
		free(escaped_filename);
		(void)show_error_msg("Unable to create FUSE mount directory", mount_point);
		return NULL;
	}
	free(escaped_filename);

	escaped_path = escape_filename(filename, 0);
	escaped_mount_point = escape_filename(mount_point, 0);

	/* I need the return code of the mount command */
	status_bar_message("FUSE mounting selected file, please stand by..");
	buf_pos = buf;
	prog_pos = program;
	/* Build the mount command based on the FUSE program config line in vifmrc.
		 Accepted FORMAT: FUSE_MOUNT|some_mount_command %SOURCE_FILE %DESTINATION_DIR
		 or
		 FUSE_MOUNT2|some_mount_command %PARAM %DESTINATION_DIR */
	strcpy(buf_pos, "");
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
			if(buf_pos + strlen(escaped_path) >= buf + sizeof(buf) + 2)
				continue;
			else if(!strcmp(cmd_buf, "%SOURCE_FILE") || !strcmp(cmd_buf, "%PARAM"))
			{
				strcpy(buf_pos, escaped_path);
				buf_pos += strlen(escaped_path);
			}
			else if(!strcmp(cmd_buf, "%DESTINATION_DIR"))
			{
				strcpy(buf_pos, escaped_mount_point);
				buf_pos += strlen(escaped_mount_point);
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

	*buf_pos = '\0';
	free(escaped_mount_point);
	free(escaped_path);
	/* CMD built */
	/* Just before running the mount,
		 I need to chdir out temporarily from any FUSE mounted
		 paths, Otherwise the fuse-zip command fails with
		 "fusermount: failed to open current directory: permission denied"
		 (this happens when mounting JARs from mounted JARs) */
	if(chdir(cfg.fuse_home) != 0)
	{
		(void)show_error_msg("FUSE MOUNT ERROR", "Can't chdir() to FUSE home");
		return NULL;
	}

	if(clear_before_mount)
	{
		def_prog_mode();
		endwin();
	}

	tmp_file = make_name_unique("/tmp/vifm.errors");
	strcat(buf, " 2> ");
	strcat(buf, tmp_file);
	status = background_and_wait_for_status(buf);
	/* check child status */
	if(!WIFEXITED(status) || (WIFEXITED(status) && WEXITSTATUS(status)))
	{
		FILE *ef = fopen(tmp_file, "r");
		print_errors(ef);
		unlink(tmp_file);

		werase(status_bar);
		/* remove the DIR we created for the mount */
		if(!access(mount_point, F_OK))
			rmdir(mount_point);
		(void)show_error_msg("FUSE MOUNT ERROR", filename);
		(void)chdir(view->curr_dir);
		return NULL;
	}
	unlink(tmp_file);
	status_bar_message("FUSE mount success");

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

	return fuse_item;
}

/* wont mount same file twice */
static void
fuse_try_mount(FileView *view, const char *program)
{
	Fuse_List *runner;
	char filename[PATH_MAX];
	char mount_point[PATH_MAX];
	int mount_found;

	if(access(cfg.fuse_home, F_OK|W_OK|X_OK) != 0)
	{
#ifndef _WIN32
		if(mkdir(cfg.fuse_home, S_IRWXU))
#else
		if(mkdir(cfg.fuse_home))
#endif
		{
			(void)show_error_msg("Unable to create FUSE mount home directory",
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
		Fuse_List *item;
		/* new file to be mounted */
		if(strncmp(program, "FUSE_MOUNT2", 11) == 0)
		{
			FILE *f;
			size_t len;
			if((f = fopen(filename, "r")) == NULL)
			{
				(void)show_error_msg("SSH mount failed", "Can't open file for reading");
				curr_stats.save_msg = 1;
				return;
			}

			if(fgets(filename, sizeof(filename), f) == NULL)
			{
				(void)show_error_msg("SSH mount failed", "Can't read file content");
				curr_stats.save_msg = 1;
				fclose(f);
				return;
			}
			len = strlen(filename);

			if(len == 0 || (len == 1 && filename[0] == '\n'))
			{
				(void)show_error_msg("SSH mount failed", "File is empty");
				curr_stats.save_msg = 1;
				return;
			}

			if(filename[len - 1] == '\n')
				filename[len - 1] = '\0';

			fclose(f);
		}
		if((item = fuse_mount(view, filename, program, mount_point)) != NULL)
			snprintf(item->source_file_name, PATH_MAX, "%s/%s", view->curr_dir,
					get_current_file_name(view));
		else
			return;
	}

	if(change_directory(view, mount_point) >= 0)
	{
		load_dir_list(view, 0);
		move_to_list_pos(view, view->curr_line);
	}
}

static int
multi_run_compat(FileView *view, const char *program)
{
	size_t len;
	if(program == NULL)
		return 0;
	if(view->selected_files <= 1)
		return 0;
	if((len = strlen(program)) == 0)
		return 0;
	if(program[len - 1] != '&')
		return 0;
	if(strstr(program, "%f") != NULL || strstr(program, "%F") != NULL)
		return 0;
	if(strstr(program, "%c") == NULL && strstr(program, "%C") == NULL)
		return 0;
	return 1;
}

static void
handle_dir(FileView *view)
{
	char *filename;
	
	filename = get_current_file_name(view);

	if(strcmp(filename, "../") == 0)
	{
		cd_updir(view);
	}
	else if(change_directory(view, filename) >= 0)
	{
		load_dir_list(view, 0);
		move_to_list_pos(view, view->list_pos);
	}
}

static void
execute_file(FileView *view, int dont_execute)
{
	char *program = NULL;
	int undef;
	int same;
	int i;
	int no_multi_run = 0;

	if(!view->dir_entry[view->list_pos].selected)
		clean_selected_files(view);

	program = get_default_program_for_file(view->dir_entry[view->list_pos].name);
	no_multi_run += !multi_run_compat(view, program);
	undef = 0;
	same = 1;
	for(i = 0; i < view->list_rows; i++)
	{
		char *prog;

		if(!view->dir_entry[i].selected)
			continue;

		if(access(view->dir_entry[i].name, F_OK) != 0)
		{
			show_error_msgf("Broken Link", "Destination of \"%s\" link doesn't exist",
					view->dir_entry[i].name);
			free(program);
			return;
		}

		prog = get_default_program_for_file(view->dir_entry[i].name);
		if(prog != NULL)
		{
			no_multi_run += !multi_run_compat(view, prog);
			if(program == NULL)
				program = prog;
			else
			{
				if(strcmp(prog, program) != 0)
					same = 0;
				free(prog);
			}
		}
		else
			undef++;
	}

	if(!same && undef == 0 && no_multi_run)
	{
		free(program);
		(void)show_error_msg("Selection error", "Files have different programs");
		return;
	}
	if(undef > 0)
	{
		free(program);
		program = NULL;
	}

	/* Check for a filetype */
	/* vi is set as the default for any extension without a program */
	if(program == NULL)
	{
		if(view->dir_entry[view->list_pos].type == DIRECTORY)
		{
			handle_dir(view);
		}
		else if(view->selected_files <= 1)
		{
			view_file(get_current_file_name(view), -1);
		}
		else
		{
			int bg;
			program = edit_selection(view, &bg);
			if(bg)
				start_background_job(program);
			else
				shellout(program, -1);
			free(program);
		}
		return;
	}

	if(!no_multi_run)
	{
		int pos = view->list_pos;
		free(program);
		for(i = 0; i < view->list_rows; i++)
		{
			if(!view->dir_entry[i].selected)
				continue;
			view->list_pos = i;
			program = get_default_program_for_file(
					view->dir_entry[view->list_pos].name);
			run_using_prog(view, program, dont_execute, 0);
			free(program);
		}
		view->list_pos = pos;
	}
	else
	{
		run_using_prog(view, program, dont_execute, 0);
		free(program);
	}
}

void
run_using_prog(FileView *view, const char *program, int dont_execute,
		int force_background)
{
	int pause = strncmp(program, "!!", 2) == 0;
	if(pause)
		program += 2;

	if(access(view->dir_entry[view->list_pos].name, F_OK) != 0)
	{
		(void)show_error_msg("Broken Link", "Link destination doesn't exist");
		return;
	}

	if(strncmp(program, "FUSE_MOUNT", 10) == 0
			|| strncmp(program, "FUSE_MOUNT2", 11) == 0)
	{
		if(dont_execute)
			view_file(get_current_file_name(view), -1);
		else
			fuse_try_mount(view, program);
	}
	else if(strchr(program, '%') != NULL)
	{
		int use_menu = 0, split = 0;
		size_t len;
		int background;
		char *command = expand_macros(view, program, NULL, &use_menu, &split);

		len = strlen(command);
		background = len > 1 && command[len - 1] == '&' && command[len - 2] == ' ';
		if(background)
			command[len - 2] = '\0';

		if(!pause && (background || force_background))
			start_background_job(command);
		else
			shellout(command, pause ? 1 : -1);

		free(command);
	}
	else
	{
		char buf[NAME_MAX + 1 + NAME_MAX + 1];
		char *temp = escape_filename(view->dir_entry[view->list_pos].name, 0);

		snprintf(buf, sizeof(buf), "%s %s", program, temp);
		shellout(buf, pause ? 1 : -1);
		free(temp);
	}
}

static void
follow_link(FileView *view, int follow_dirs)
{
	struct stat s;
	int is_dir = 0;
	char *dir = NULL, *file = NULL, *link_dup;
	char linkto[PATH_MAX + NAME_MAX];
	char *filename;

	filename = view->dir_entry[view->list_pos].name;

	if(get_link_target(filename, linkto, sizeof(linkto)) != 0)
	{
		(void)show_error_msg("Error", "Can't read link");
		return;
	}

	if(access(linkto, F_OK) != 0)
	{
		(void)show_error_msg("Broken Link",
				"Can't access link destination. It might be broken");
		curr_stats.save_msg = 1;
		return;
	}

	chosp(linkto);
	link_dup = strdup(linkto);

	lstat(linkto, &s);

	if((s.st_mode & S_IFMT) == S_IFDIR && !follow_dirs)
	{
		is_dir = 1;
		dir = strdup(view->dir_entry[view->list_pos].name);
	}
	else
	{
		int x;
		for(x = strlen(linkto) - 1; x > 0; x--)
		{
			if(linkto[x] == '/')
			{
				struct stat s;
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
			file++;
		else if(is_dir == 0)
			file = link_dup;
	}
	if(is_dir)
	{
		if(change_directory(view, dir) >= 0)
		{
			load_dir_list(view, 0);
			move_to_list_pos(view, view->curr_line);
		}
	}
	if(file != NULL)
	{
		int pos;
		if((s.st_mode & S_IFMT) == S_IFDIR)
		{
			size_t len;

			chosp(link_dup);
			len = strlen(link_dup);
			link_dup = realloc(link_dup, len + 1 + 1);
			if((file = strrchr(link_dup, '/')) == NULL)
				file = link_dup;
			else
				file++;
			strcat(file, "/");
		}
		pos = find_file_pos_in_list(view, file);
		if(pos >= 0)
			move_to_list_pos(view, pos);
	}
	free(link_dup);
	free(dir);
}

void
handle_file(FileView *view, int dont_execute, int force_follow)
{
	char name[NAME_MAX];
	int type;
	int executable;
	int runnable;
	char *filename;
	
	filename = get_current_file_name(view);
	snprintf(name, sizeof(name), "%s", filename);
	chosp(name);

	type = view->dir_entry[view->list_pos].type;

	if(view->selected_files > 1)
	{
		int files = 0, dirs = 0;
		int i;
		for(i = 0; i < view->list_rows; i++)
		{
			int type = view->dir_entry[i].type;
			if(!view->dir_entry[i].selected)
				continue;
			if(type == DIRECTORY || (type == LINK && is_dir(view->dir_entry[i].name)))
				dirs++;
			else
				files++;
		}
		if(dirs > 0 && files > 0)
		{
			(void)show_error_msg("Selection error",
					"Selection cannot contain files and directories at the same time");
			return;
		}
	}

#ifndef _WIN32
	if(is_dir(name))
#else
	if((is_dir(name) || is_unc_root(view->curr_dir)))
#endif
	{
		if(view->selected_files == 0 &&
				(view->dir_entry[view->list_pos].type != LINK || !force_follow))
		{
			handle_dir(view);
			return;
		}
	}

	runnable = !cfg.follow_links && type == LINK && !check_link_is_dir(filename);
	if(runnable && force_follow)
		runnable = 0;
	if(view->selected_files > 0)
		runnable = 1;
	runnable = type == REGULAR || type == EXECUTABLE || runnable ||
			type == DIRECTORY;

	executable = type == EXECUTABLE || (runnable && access(filename, X_OK) == 0 &&
			S_ISEXE(view->dir_entry[view->list_pos].mode));
	executable = executable && !dont_execute && cfg.auto_execute;

	if(cfg.vim_filter && (executable || runnable))
		use_vim_plugin(view, 0, NULL); /* no return */

	if(executable)
	{
		char buf[NAME_MAX];
		snprintf(buf, sizeof(buf), "%s/%s", view->curr_dir, filename);
#ifndef _WIN32
		shellout(buf, 1);
#else
		system(buf);
#endif
	}
	else if(runnable)
	{
		execute_file(view, dont_execute);
	}
	else if(type == LINK)
	{
		follow_link(view, force_follow);
	}
}

void _gnuc_noreturn
use_vim_plugin(FileView *view, int argc, char **argv)
{
	FILE *fp;
	char filepath[PATH_MAX] = "";

	snprintf(filepath, sizeof(filepath), "%s/vimfiles", cfg.config_dir);
	fp = fopen(filepath, "w");
	if(argc == 0)
	{
		if(!view->dir_entry[view->list_pos].selected)
		{
			fprintf(fp, "%s/%s\n", view->curr_dir,
					view->dir_entry[view->list_pos].name);
		}
		else
		{
			int i;

			for(i = 0; i < view->list_rows; i++)
				if(view->dir_entry[i].selected)
					fprintf(fp, "%s/%s\n", view->curr_dir, view->dir_entry[i].name);
		}
	}
	else
	{
		int i;
		for(i = 0; i < argc; i++)
			if(argv[i][0] == '/')
				fprintf(fp, "%s\n", argv[i]);
			else
				fprintf(fp, "%s/%s\n", view->curr_dir, argv[i]);
	}
	fclose(fp);

	write_info_file();

	endwin();
	exit(0);
}

static void
progress_msg(const char *text, int ready, int total)
{
	char msg[strlen(text) + 32];

	sprintf(msg, "%s %d/%d", text, ready, total);
	show_progress(msg, 1);
}

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

int
is_dir_writable(int dest, const char *path)
{
	if(access(path, W_OK) == 0)
		return 1;
	if(dest)
		(void)show_error_msg("Operation error",
				"Destination directory is not writable");
	else
		(void)show_error_msg("Operation error",
				"Current directory is not writable");
	return 0;
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
			strncat(buf, ", ", COMMAND_GROUP_INFO_LEN);
			buf[COMMAND_GROUP_INFO_LEN - 1] = '\0';
		}
		strncat(buf, list[i], COMMAND_GROUP_INFO_LEN);
		buf[COMMAND_GROUP_INFO_LEN - 1] = '\0';
		len = strlen(buf);
	}
}

/* returns new value for save_msg */
int
delete_file(FileView *view, int reg, int count, int *indexes, int use_trash)
{
	char buf[8 + PATH_MAX*2];
	int x, y;
	int i;

	if(!is_dir_writable(0, view->curr_dir))
		return 0;

	if(cfg.use_trash && use_trash &&
			path_starts_with(view->curr_dir, cfg.trash_dir))
	{
		(void)show_error_msg("Can't perform deletion",
				"Current directory is under Trash directory");
		return 1;
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
		if(view->selected_files == 0)
		{
			view->dir_entry[view->list_pos].selected = 1;
			view->selected_files = 1;
		}
		get_all_selected_files(view);
		i = view->list_pos;
		while(i < view->list_rows - 1 && view->dir_entry[i].selected)
			i++;
	}
	view->list_pos = i;

	if(cfg.use_trash && use_trash)
	{
		/* A - Z  append to register otherwise replace */
		if(reg >= 'A' && reg <= 'Z')
			reg += 'a' - 'A';
		else
			clear_register(reg);
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
	if(chdir(curr_view->curr_dir) != 0)
	{
		(void)show_error_msg("Directory return",
				"Can't chdir() to current directory");
		return 1;
	}
	for(x = 0; x < view->selected_files; x++)
	{
		char full_buf[PATH_MAX];
		int result;

		if(strcmp("../", view->selected_filelist[x]) == 0)
		{
			(void)show_error_msg("Background Process Error",
					"You cannot delete the ../ directory");
			continue;
		}

		snprintf(full_buf, sizeof(full_buf), "%s/%s", view->curr_dir,
				view->selected_filelist[x]);
		chosp(full_buf);

		progress_msg("Deleting files", x + 1, view->selected_files);
		if(cfg.use_trash && use_trash)
		{
			if(strcmp(full_buf, cfg.trash_dir) == 0)
			{
				(void)show_error_msg("Background Process Error",
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

	move_to_list_pos(view, view->list_pos);

	status_bar_messagef("%d %s deleted", y, y == 1 ? "file" : "files");
	return 1;
}

static int
mv_file(const char *src, const char *dst, const char *path, int tmpfile_num)
{
	char full_src[PATH_MAX], full_dst[PATH_MAX];
	int op;
	int result;

	snprintf(full_src, sizeof(full_src), "%s/%s", curr_view->curr_dir, src);
	chosp(full_src);
	snprintf(full_dst, sizeof(full_dst), "%s/%s", path, dst);
	chosp(full_dst);

	if(strcmp(full_src, full_dst) == 0)
		return 0;

	if(tmpfile_num == 0)
		op = OP_MOVE;
	else if(tmpfile_num == -1)
		op = OP_MOVETMP0;
	else if(tmpfile_num == 1)
		op = OP_MOVETMP1;
	else if(tmpfile_num == 2)
		op = OP_MOVETMP2;
	else
		op = OP_NONE;

	result = perform_operation(op, NULL, full_src, full_dst);
	if(result == 0)
		add_operation(op, NULL, NULL, full_src, full_dst);
	return result;
}

static void
rename_file_cb(const char *new_name)
{
	char *filename = get_current_file_name(curr_view);
	char buf[10 + NAME_MAX + 1];
	char new[NAME_MAX + 1];
	size_t len;
	int tmp;

	if(new_name == NULL || new_name[0] == '\0')
		return;

	if(strchr(new_name, '/') != 0)
	{
		status_bar_error("Name can not contain slash");
		curr_stats.save_msg = 1;
		return;
	}

	len = strlen(filename);
	snprintf(new, sizeof(new), "%s%s%s", new_name,
			(filename[len - 1] == '/') ? "/" : "", rename_file_ext);

	/* Filename unchanged */
	if(strcmp(filename, new) == 0)
		return;

	if(access(new, F_OK) == 0 && strncmp(filename, new, strlen(filename)) != 0)
	{
		(void)show_error_msg("File exists",
				"That file already exists. Will not overwrite.");
		return;
	}

	snprintf(buf, sizeof(buf), "rename in %s: %s to %s",
			replace_home_part(curr_view->curr_dir), filename, new);
	cmd_group_begin(buf);
	tmp = mv_file(filename, new, curr_view->curr_dir, 0);
	cmd_group_end();
	if(tmp != 0)
		return;

	free(curr_view->dir_entry[curr_view->list_pos].name);
	curr_view->dir_entry[curr_view->list_pos].name = strdup(new);
	load_saving_pos(curr_view, 1);
}

void
rename_file(FileView *view, int name_only)
{
	char* p;
	char buf[NAME_MAX + 1];

	if(!is_dir_writable(0, view->curr_dir))
		return;

	strncpy(buf, get_current_file_name(view), sizeof(buf));
	buf[sizeof(buf) - 1] = '\0';
	if(strcmp(buf, "../") == 0)
	{
		(void)show_error_msg("Rename error",
				"You can't rename parent directory this way");
		return;
	}

	chosp(buf);

	if(!name_only || (p = strchr(buf, '.')) == NULL)
	{
		rename_file_ext[0] = '\0';
	}
	else
	{
		strcpy(rename_file_ext, p);
		*p = '\0';
	}

	clean_selected_files(view);
	enter_prompt_mode(L"New name: ", buf, rename_file_cb);
}

static char **
read_file_lines(FILE *f, int *nlines)
{
	char **list = NULL;
	char name[NAME_MAX];

	*nlines = 0;
	while(fgets(name, sizeof(name), f) != NULL)
		*nlines = add_to_string_array(&list, *nlines, 1, name);
	return list;
}

static int
is_name_list_ok(int count, int nlines, char **list)
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

		if(strchr(list[i], '/') != NULL)
		{
			status_bar_errorf("Name \"%s\" contains slash", list[i]);
			curr_stats.save_msg = 1;
			return 0;
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

static int
is_rename_list_ok(FileView *view, int *indexes, int count, char **list)
{
	int i;

	for(i = 0; i < count; i++)
	{
		int j;

		if(list[i][0] == '\0')
			continue;

		if(strcmp(list[i], view->dir_entry[abs(indexes[i])].name) == 0)
			continue;

		for(j = 0; j < count; j++)
		{
			chosp(view->dir_entry[abs(indexes[j])].name);
			if(strcmp(list[i], view->dir_entry[abs(indexes[j])].name) == 0 &&
					indexes[j] >= 0)
			{
				indexes[j] = -indexes[j];
				break;
			}
		}
		if(j >= count && access(list[i], F_OK) == 0)
		{
			status_bar_errorf("File \"%s\" already exists", list[i]);
			curr_stats.save_msg = 1;
			return 0;
		}
	}

	return 1;
}

/* Returns count of renamed files */
static int
perform_renaming(FileView *view, int *indexes, int count, char **list)
{
	char buf[10 + NAME_MAX + 1];
	size_t len;
	int i;
	int renamed = 0;

	snprintf(buf, sizeof(buf), "rename in %s: ",
			replace_home_part(view->curr_dir));
	len = strlen(buf);

	for(i = 0; i < count && len < COMMAND_GROUP_INFO_LEN; i++)
	{
		if(buf[len - 2] != ':')
		{
			strncat(buf, ", ", sizeof(buf));
			buf[sizeof(buf) - 1] = '\0';
		}
		len = strlen(buf);
		len += snprintf(buf + len, sizeof(buf) - len, "%s to %s",
				view->dir_entry[abs(indexes[i])].name, list[i]);
	}

	cmd_group_begin(buf);

	for(i = 0; i < count; i++)
	{
		const char *tmp;
		int ind;

		if(list[i][0] == '\0')
			continue;
		if(strcmp(list[i], view->dir_entry[abs(indexes[i])].name) == 0)
			continue;
		if(indexes[i] >= 0)
			continue;

		ind = -indexes[i];

		tmp = make_name_unique(view->dir_entry[ind].name);
		if(mv_file(view->dir_entry[ind].name, tmp, view->curr_dir, 2) != 0)
		{
			cmd_group_end();
			undo_group();
			status_bar_error("Temporary rename error");
			curr_stats.save_msg = 1;
			return 0;
		}
		free(view->dir_entry[ind].name);
		view->dir_entry[ind].name = strdup(tmp);
	}

	for(i = 0; i < count; i++)
	{
		if(list[i][0] == '\0')
			continue;
		if(strcmp(list[i], view->dir_entry[abs(indexes[i])].name) == 0)
			continue;

		if(mv_file(view->dir_entry[abs(indexes[i])].name, list[i], view->curr_dir,
				(indexes[i] < 0) ? 1 : -1) == 0)
		{
			int pos;

			renamed++;

			pos = find_file_pos_in_list(view, view->dir_entry[abs(indexes[i])].name);
			if(pos == view->list_pos)
			{
				free(view->dir_entry[pos].name);
				view->dir_entry[pos].name = strdup(list[i]);
			}
		}
	}

	cmd_group_end();

	return renamed;
}

#ifdef _WIN32
static void
change_slashes(char *str)
{
	int i;
	for(i = 0; str[i] != '\0'; i++)
	{
		if(str[i] == '\\')
			str[i] = '/';
	}
}
#endif

static char **
read_list_from_file(int count, char **names, int *nlines, int require_change)
{
	char temp_file[PATH_MAX];
	char **list;
	FILE *f;
	int i;

#ifndef _WIN32
	snprintf(temp_file, sizeof(temp_file), "/tmp/vifm.rename");
#else
	snprintf(temp_file, sizeof(temp_file), "%s\\vifm.rename", getenv("TMP"));
	change_slashes(temp_file);
#endif
	strncpy(temp_file, make_name_unique(temp_file), sizeof(temp_file));

	if((f = fopen(temp_file, "w")) == NULL)
	{
		status_bar_error("Can't create temp file");
		curr_stats.save_msg = 1;
		return NULL;
	}

	for(i = 0; i < count; i++)
	{
		char *name = names[i];
		size_t len = strlen(name);
		int slash = name[len - 1] == '/';
		if(slash)
			name[len - 1] = '\0';
		fprintf(f, "%s\n", name);
		if(slash)
			name[len - 1] = '/';
	}

	fclose(f);

	if(require_change)
	{
		struct stat st_before, st_after;

		stat(temp_file, &st_before);

		view_file(temp_file, -1);

		stat(temp_file, &st_after);

		if(memcmp(&st_after.st_mtime, &st_before.st_mtime,
				sizeof(st_after.st_mtime)) == 0)
		{
			unlink(temp_file);
			curr_stats.save_msg = 0;
			return NULL;
		}
	}
	else
	{
		view_file(temp_file, -1);
	}

	if((f = fopen(temp_file, "r")) == NULL)
	{
		unlink(temp_file);
		status_bar_error("Can't open temporary file");
		curr_stats.save_msg = 1;
		return NULL;
	}

	list = read_file_lines(f, nlines);
	fclose(f);
	unlink(temp_file);

	curr_stats.save_msg = 0;
	return list;
}

static void
rename_files_ind(FileView *view, int *indexes, int count)
{
	char **list;
	char **names;
	int n;
	int nlines, i, renamed = -1;

	if(count == 0)
	{
		status_bar_message("0 files renamed");
		return;
	}

	names = NULL;
	n = 0;
	for(i = 0; i < count; i++)
	{
		char *name = view->dir_entry[indexes[i]].name;
		n = add_to_string_array(&names, n, 1, name);
	}

	if((list = read_list_from_file(count, names, &nlines, 1)) == NULL)
	{
		free_string_array(names, count);
		status_bar_message("0 files renamed");
		return;
	}
	free_string_array(names, count);

	if(is_name_list_ok(count, nlines, list) &&
			is_rename_list_ok(view, indexes, count, list))
		renamed = perform_renaming(view, indexes, count, list);
	free_string_array(list, nlines);

	if(renamed >= 0)
		status_bar_messagef("%d file%s renamed", renamed,
				(renamed == 1) ? "" : "s");
}

int
rename_files(FileView *view, char **list, int nlines)
{
	int *indexes;
	int count;
	int i, j;

	if(!is_dir_writable(0, view->curr_dir))
		return 0;

	if(view->selected_files == 0)
	{
		view->dir_entry[view->list_pos].selected = 1;
		view->selected_files = 1;
	}

	count = view->selected_files;
	indexes = malloc(sizeof(*indexes)*count);
	if(indexes == NULL)
	{
		(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
		return 0;
	}

	j = 0;
	for(i = 0; i < view->list_rows; i++)
	{
		if(!view->dir_entry[i].selected)
			continue;
		else if(strcmp(view->dir_entry[i].name, "../") == 0)
			count--;
		else
			indexes[j++] = i;
	}

	if(nlines == 0)
	{
		rename_files_ind(view, indexes, count);
	}
	else
	{
		int renamed = -1;

		if(is_name_list_ok(count, nlines, list) &&
				is_rename_list_ok(view, indexes, count, list))
			renamed = perform_renaming(view, indexes, count, list);

		if(renamed >= 0)
			status_bar_messagef("%d file%s renamed", renamed,
					(renamed == 1) ? "" : "s");
	}

	free(indexes);

	clean_selected_files(view);
	draw_dir_list(view, view->top_line);
	move_to_list_pos(view, view->list_pos);
	curr_stats.save_msg = 1;
	return 1;
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

void
change_owner(void)
{
	if(curr_view->selected_filelist == 0)
	{
		curr_view->dir_entry[curr_view->list_pos].selected = 1;
		curr_view->selected_files = 1;
	}
	clean_selected_files(curr_view);
	enter_prompt_mode(L"New owner: ", "", change_owner_cb);
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

void
change_group(void)
{
	if(curr_view->selected_filelist == 0)
	{
		curr_view->dir_entry[curr_view->list_pos].selected = 1;
		curr_view->selected_files = 1;
	}
	clean_selected_files(curr_view);
	enter_prompt_mode(L"New group: ", "", change_group_cb);
}

static void
change_link_cb(const char *new_target)
{
	char buf[PATH_MAX];
	char linkto[PATH_MAX];
	const char *filename;

	if(new_target == NULL || new_target[0] == '\0')
		return;

	curr_stats.confirmed = 1;

	filename = curr_view->dir_entry[curr_view->list_pos].name;
	if(get_link_target(filename, linkto, sizeof(linkto)) != 0)
	{
		(void)show_error_msg("Error", "Can't read link");
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

int
change_link(FileView *view)
{
	char linkto[PATH_MAX];

	if(!is_dir_writable(0, view->curr_dir))
		return 0;

	if(view->dir_entry[view->list_pos].type != LINK)
	{
		status_bar_error("File isn't a symbolic link");
		return 1;
	}

	if(get_link_target(view->dir_entry[view->list_pos].name, linkto,
			sizeof(linkto)) != 0)
	{
		(void)show_error_msg("Error", "Can't read link");
		return 0;
	}

	enter_prompt_mode(L"Link target: ", linkto, change_link_cb);
	return 0;
}

static void
prompt_dest_name(const char *src_name)
{
	wchar_t buf[256];

#ifndef _WIN32
	swprintf(buf, ARRAY_LEN(buf), L"New name for %s: ", src_name);
#else
	swprintf(buf, L"New name for %S: ", src_name);
#endif
	enter_prompt_mode(buf, src_name, put_confirm_cb);
}

static void
prompt_what_to_do(const char *src_name)
{
	wchar_t buf[NAME_MAX];

	free(put_confirm.name);
	put_confirm.name = strdup(src_name);
#ifndef _WIN32
	swprintf(buf, sizeof(buf)/sizeof(buf[0]),
			L"Name conflict for %s. [r]ename/[s]kip/[o]verwrite/overwrite [a]ll: ",
			src_name);
#else
	swprintf(buf,
			L"Name conflict for %S. [r]ename/[s]kip/[o]verwrite/overwrite [a]ll: ",
			src_name);
#endif
	enter_prompt_mode(buf, "", put_decide_cb);
}

/* Returns 0 on success */
static int
put_next(const char *dest_name, int override)
{
	char *filename;
	struct stat st;
	char src_buf[PATH_MAX], dst_buf[PATH_MAX];
	int from_trash;
	int op;
	int move;

	/* TODO: refactor this function (put_next()) */

	override = override || put_confirm.overwrite_all;

	filename = put_confirm.reg->files[put_confirm.x];
	chosp(filename);
	if(lstat(filename, &st) != 0)
		return 0;

	from_trash = strncmp(filename, cfg.trash_dir, strlen(cfg.trash_dir)) == 0;
	move = from_trash || put_confirm.force_move;

	if(dest_name[0] == '\0')
		dest_name = strrchr(filename, '/') + 1;

	strcpy(src_buf, filename);
	chosp(src_buf);

	if(from_trash)
	{
		while(isdigit(*dest_name))
			dest_name++;
		dest_name++;
	}

	if(access(dest_name, F_OK) == 0 && !override)
	{
		prompt_what_to_do(dest_name);
		return 1;
	}

	snprintf(dst_buf, sizeof(dst_buf), "%s/%s", put_confirm.view->curr_dir,
			dest_name);
	chosp(dst_buf);

	if(override)
	{
		struct stat st;
		if(lstat(dest_name, &st) == 0)
		{
			if(perform_operation(OP_REMOVESL, NULL, dst_buf, NULL) != 0)
				return 0;
		}

		memset(&put_confirm.view->dir_mtime, 0,
				sizeof(put_confirm.view->dir_mtime));
	}

	if(put_confirm.link)
	{
		op = OP_SYMLINK;
		if(put_confirm.link == 2)
			strcpy(src_buf, make_rel_path(filename, put_confirm.view->curr_dir));
	}
	else if(move)
	{
		op = OP_MOVE;
	}
	else
	{
		op = OP_COPY;
	}

	progress_msg("Putting files", put_confirm.x + 1, put_confirm.reg->num_files);
	if(perform_operation(op, NULL, src_buf, dst_buf) == 0)
	{
		char *msg;
		size_t len;

		cmd_group_continue();

		msg = replace_group_msg(NULL);
		len = strlen(msg);
		msg = realloc(msg, COMMAND_GROUP_INFO_LEN);

		snprintf(msg + len, COMMAND_GROUP_INFO_LEN - len, "%s%s",
				(msg[len - 2] != ':') ? ", " : "", dest_name);
		replace_group_msg(msg);

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
	else
	{
		prompt_what_to_do(put_confirm.name);
	}
}

/* Returns new value for save_msg flag. */
static int
put_files_from_register_i(FileView *view, int start)
{
	if(start)
	{
		char buf[PATH_MAX + NAME_MAX*2 + 4];
		const char *op = "UNKNOWN";
		int from_trash = strncmp(put_confirm.reg->files[0], cfg.trash_dir,
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

	if(chdir(view->curr_dir) != 0)
	{
		(void)show_error_msg("Directory Return",
				"Can't chdir() to current directory");
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

	return 1;
}

/* Returns new value for save_msg flag. */
int
put_files_from_register(FileView *view, int name, int force_move)
{
	registers_t *reg;

	if(!is_dir_writable(0, view->curr_dir))
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
	return put_files_from_register_i(view, 1);
}

static void
clone_file(FileView* view, const char *filename, const char *path,
		const char *clone)
{
	char full[PATH_MAX];
	char clone_name[PATH_MAX];
	
	if(strcmp(filename, "./") == 0)
		return;
	if(strcmp(filename, "../") == 0)
		return;

	if(clone == NULL)
	{
		int i;
		size_t len;

		snprintf(clone_name, sizeof(clone_name), "%s/%s", path, filename);
		chosp(clone_name);
		i = 1;
		len = strlen(clone_name);
		while(access(clone_name, F_OK) == 0)
			snprintf(clone_name + len, sizeof(clone_name) - len, "(%d)", i++);
	}
	else
	{
		snprintf(clone_name, sizeof(clone_name), "%s/%s", path, clone);
		chosp(clone_name);
		if(access(clone_name, F_OK) == 0)
		{
			if(perform_operation(OP_REMOVESL, NULL, clone_name, NULL) != 0)
				return;
		}
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
		if(access(list[i], F_OK) == 0)
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
		char *tmp = expand_tilde(strdup(path));
		strcpy(buf, tmp);
		free(tmp);
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

static int
have_read_access(FileView *view)
{
	int i;
	for(i = 0; i < view->list_rows; i++)
	{
		if(!view->dir_entry[i].selected)
			continue;
		if(access(view->dir_entry[i].name, R_OK) != 0)
		{
			show_error_msgf("Access denied",
					"You don't have read permissions on \"%s\"", view->dir_entry[i].name);
			clean_selected_files(view);
			draw_dir_list(view, view->top_line);
			move_to_list_pos(view, view->list_pos);
			return 0;
		}
	}
	return 1;
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
			strncat(buf, ", ", COMMAND_GROUP_INFO_LEN);
			buf[COMMAND_GROUP_INFO_LEN - 1] = '\0';
		}
		strncat(buf, view->selected_filelist[i], COMMAND_GROUP_INFO_LEN);
		if(nlines > 0)
		{
			strncat(buf, " to ", COMMAND_GROUP_INFO_LEN);
			strncat(buf, list[i], COMMAND_GROUP_INFO_LEN);
		}
		buf[COMMAND_GROUP_INFO_LEN - 1] = '\0';
		len = strlen(buf);
	}
}

/* returns new value for save_msg */
int
clone_files(FileView *view, char **list, int nlines, int force)
{
	int i;
	char buf[COMMAND_GROUP_INFO_LEN + 1];
	char path[PATH_MAX];
	int with_dir = 0;
	int from_file;

	if(view->selected_files == 0)
	{
		view->dir_entry[view->list_pos].selected = 1;
		view->selected_files = 1;
	}
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
	if(!is_dir_writable(1, path))
		return 0;

	get_all_selected_files(view);

	from_file = nlines < 0;
	if(from_file)
	{
		list = read_list_from_file(view->selected_files, view->selected_filelist,
				&nlines, 1);
		if(list == NULL)
			return curr_stats.save_msg;
	}

	if(nlines > 0 && (!is_name_list_ok(view->selected_files, nlines, list) ||
			(!force && !is_clone_list_ok(nlines, list))))
	{
		clean_selected_files(view);
		draw_dir_list(view, view->top_line);
		move_to_list_pos(view, view->list_pos);
		if(from_file)
			free_string_array(list, nlines);
		return 1;
	}

	if(with_dir)
		snprintf(buf, sizeof(buf), "clone in %s to %s: ", view->curr_dir, list[0]);
	else
		snprintf(buf, sizeof(buf), "clone in %s: ", view->curr_dir);
	make_undo_string(view, buf, nlines, list);

	cmd_group_begin(buf);
	for(i = 0; i < view->selected_files; i++)
	{
		progress_msg("Cloning files", i + 1, view->selected_files);
		clone_file(view, view->selected_filelist[i], path,
				(nlines > 0) ? list[i] : NULL);
	}
	cmd_group_end();
	free_selected_file_array(view);

	clean_selected_files(view);
	load_saving_pos(view, 1);
	load_saving_pos(other_view, 1);
	if(from_file)
		free_string_array(list, nlines);
	return 0;
}

unsigned long long
calc_dirsize(const char *path, int force_update)
{
	DIR* dir;
	struct dirent* dentry;
	const char* slash = "";
	unsigned long long size;

	dir = opendir(path);
	if(dir == NULL)
		return 0;

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
#ifndef _WIN32
		if(dentry->d_type == DT_DIR)
#else
		if(is_dir(buf))
#endif
		{
			unsigned long long dir_size = 0;
			if(tree_get_data(curr_stats.dirsize_cache, buf, &dir_size) != 0
					|| force_update)
				dir_size = calc_dirsize(buf, force_update);
			size += dir_size;
		}
		else
		{
			struct stat st;
			if(lstat(buf, &st) == 0)
				size += st.st_size;
		}
	}

	closedir(dir);

	tree_set_data(curr_stats.dirsize_cache, path, size);
	return size;
}

/* Returns new value for save_msg flag. */
int
put_links(FileView *view, int reg_name, int relative)
{
	registers_t *reg;

	if(!is_dir_writable(0, view->curr_dir))
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
	return put_files_from_register_i(view, 1);
}

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

static const char *
gsubstitute_regexp(regex_t *re, const char *src, const char *sub,
		regmatch_t *matches)
{
	static char buf[NAME_MAX];
	int off = 0;
	strcpy(buf, src);
	do
	{
		int i;
		for(i = 0; i < 10; i++)
		{
			matches[i].rm_so += off;
			matches[i].rm_eo += off;
		}

		src = substitute_regexp(buf, sub, matches, &off);
		strcpy(buf, src);

		if(matches[i].rm_eo == matches[i].rm_so)
			break;
	}while(regexec(re, buf + off, 10, matches, 0) == 0);
	return buf;
}

const char *
substitute_in_name(const char *name, const char *pattern, const char *sub,
		int glob)
{
	static char buf[PATH_MAX];
	regex_t re;
	regmatch_t matches[10];
	const char *dst;

	strcpy(buf, name);

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

	if(glob)
		dst = gsubstitute_regexp(&re, name, sub, matches);
	else
		dst = substitute_regexp(name, sub, matches, NULL);
	strcpy(buf, dst);

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
		if(strcmp(view->dir_entry[i].name, "../") == 0)
			continue;

		if(buf[len - 2] != ':')
		{
			strncat(buf, ", ", sizeof(buf));
			buf[sizeof(buf) - 1] = '\0';
		}
		strncat(buf, view->dir_entry[i].name, sizeof(buf));
		buf[sizeof(buf) - 1] = '\0';
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
		if(strcmp(view->dir_entry[i].name, "../") == 0)
			continue;

		strncpy(buf, view->dir_entry[i].name, sizeof(buf));
		chosp(buf);
		j++;
		if(strcmp(buf, dest[j]) == 0)
			continue;

		if(i == view->list_pos)
		{
			free(view->dir_entry[i].name);
			view->dir_entry[i].name = strdup(dest[j]);
		}

		mv_file(buf, dest[j], view->curr_dir, 0);
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

	if(!is_dir_writable(0, view->curr_dir))
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
		if(strcmp(view->dir_entry[i].name, "../") == 0)
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
		if(strchr(dst, '/') != NULL)
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

	if(!is_dir_writable(0, view->curr_dir))
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
		if(strcmp(view->dir_entry[i].name, "../") == 0)
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
		if(strchr(dst, '/') != NULL)
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
change_case(FileView *view, int toupper, int count, int *indexes)
{
	int i;
	char **dest = NULL;
	int n = 0, k;
	char buf[COMMAND_GROUP_INFO_LEN + 1];

	if(!is_dir_writable(0, view->curr_dir))
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
		strcpy(buf, view->selected_filelist[i]);
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
			free(view->dir_entry[pos].name);
			view->dir_entry[pos].name = strdup(dest[i]);
		}
		mv_file(view->selected_filelist[i], dest[i], view->curr_dir, 0);
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
		char buf[PATH_MAX];
		snprintf(buf, sizeof(buf), "%s/%s", dst, list[i]);
		if(access(buf, F_OK) == 0)
		{
			status_bar_errorf("File \"%s\" already exists", list[i]);
			return 0;
		}
	}
	return 1;
}

/* type:
 *  0 - copy
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

	if(type == 0)
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
	if(result == 0)
		add_operation(op, NULL, NULL, full_src, full_dst);
	return result;
}

int
cpmv_files(FileView *view, char **list, int nlines, int move, int type,
		int force)
{
	int i;
	char buf[COMMAND_GROUP_INFO_LEN + 1];
	char path[PATH_MAX];
	int from_file;
	int from_trash;

	if(move && access(view->curr_dir, W_OK) != 0)
	{
		(void)show_error_msg("Operation error",
				"Current directory is not writable");
		return 0;
	}

	if(view->selected_files == 0)
	{
		view->dir_entry[view->list_pos].selected = 1;
		view->selected_files = 1;
	}
	if(move == 0 && type == 0 && !have_read_access(view))
		return 0;

	if(nlines == 1)
	{
		if(is_dir_path(other_view, list[0], path))
			nlines = 0;
	}
	else
	{
		strcpy(path, other_view->curr_dir);
	}
	if(!is_dir_writable(1, path))
		return 0;

	get_all_selected_files(view);

	from_file = nlines < 0;
	if(from_file)
	{
		list = read_list_from_file(view->selected_files, view->selected_filelist,
				&nlines, 0);
		if(list == NULL)
			return curr_stats.save_msg;
	}

	if(nlines > 0 && (!is_name_list_ok(view->selected_files, nlines, list) ||
			(!is_copy_list_ok(path, nlines, list) && !force)))
	{
		clean_selected_files(view);
		draw_dir_list(view, view->top_line);
		move_to_list_pos(view, view->list_pos);
		if(from_file)
			free_string_array(list, nlines);
		return 1;
	}
	else if(nlines == 0 && !force &&
			!is_copy_list_ok(path, view->selected_files, view->selected_filelist))
	{
		clean_selected_files(view);
		draw_dir_list(view, view->top_line);
		move_to_list_pos(view, view->list_pos);
		if(from_file)
			free_string_array(list, nlines);
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
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " from %s to ",
			replace_home_part(view->curr_dir));
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s: ",
			replace_home_part(path));
	make_undo_string(view, buf, nlines, list);

	if(move)
	{
		i = view->list_pos;
		while(i < view->list_rows - 1 && view->dir_entry[i].selected)
			i++;
		view->list_pos = i;
	}

	from_trash = path_starts_with(view->curr_dir, cfg.trash_dir);
	cmd_group_begin(buf);
	for(i = 0; i < view->selected_files; i++)
	{
		char dst_full[PATH_MAX];
		const char *dst = (nlines > 0) ? list[i] : view->selected_filelist[i];
		if(from_trash)
		{
			while(isdigit(*dst))
				dst++;
			dst++;
		}

		snprintf(dst_full, sizeof(dst_full), "%s/%s", path, dst);
		if(access(dst_full, F_OK) == 0)
			perform_operation(OP_REMOVESL, NULL, dst_full, NULL);

		if(move)
		{
			progress_msg("Moving files", i + 1, view->selected_files);

			if(mv_file(view->selected_filelist[i], dst, path, 0) != 0)
				view->list_pos = find_file_pos_in_list(view,
						view->selected_filelist[i]);
		}
		else
		{
			if(type == 0)
				progress_msg("Copying files", i + 1, view->selected_files);
			cp_file(view->curr_dir, path, view->selected_filelist[i], dst, type);
		}
	}
	cmd_group_end();

	free_selected_file_array(view);
	clean_selected_files(view);
	load_saving_pos(view, 1);
  load_saving_pos(other_view, 1);
	if(from_file)
		free_string_array(list, nlines);
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
	draw_dir_list(view, view->top_line);
	move_to_list_pos(view, view->list_pos);
}

void
make_dirs(FileView *view, char **names, int count, int create_parent)
{
	char buf[COMMAND_GROUP_INFO_LEN + 1];
	int i;
	void *cp = (void *)(long)create_parent;

	snprintf(buf, sizeof(buf), "mkdir in %s: ",
			replace_home_part(view->curr_dir));

	get_group_file_list(names, count, buf);
	cmd_group_begin(buf);
	for(i = 0; i < count; i++)
	{
		char full[PATH_MAX];
		snprintf(full, sizeof(full), "%s/%s", view->curr_dir, names[i]);
		if(perform_operation(OP_MKDIR, cp, full, NULL) == 0)
			add_operation(OP_MKDIR, cp, NULL, full, "");
	}
	cmd_group_end();

	go_to_first_file(view, names, count);
}

int
make_files(FileView *view, char **names, int count)
{
	int i;
	char buf[COMMAND_GROUP_INFO_LEN + 1];
	size_t len;

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
			status_bar_error("One of names is empty");
			return 1;
		}
		if(strchr(names[i], '/') != NULL)
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

	len = snprintf(buf, sizeof(buf), "touch in %s: ",
			replace_home_part(view->curr_dir));

	get_group_file_list(names, count, buf);
	cmd_group_begin(buf);
	for(i = 0; i < count; i++)
	{
		char full[PATH_MAX];
		snprintf(full, sizeof(full), "%s/%s", view->curr_dir, names[i]);
		if(perform_operation(OP_MKFILE, NULL, full, NULL) == 0)
			add_operation(OP_MKFILE, NULL, NULL, full, "");
	}
	cmd_group_end();

	go_to_first_file(view, names, count);

	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
