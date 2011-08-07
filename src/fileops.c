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

#include <curses.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h> /* stat */
#include <sys/types.h> /* waitpid() */
#include <sys/wait.h> /* waitpid() */
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
} put_confirm;

static void put_confirm_cb(const char *dest_name);
static void put_decide_cb(const char *dest_name);
static int put_files_from_register_i(FileView *view, int start);

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

		tmp = escape_filename(runner->mount_point, 0, 0);
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
			linebuf[nread] = '\0';
			if(nread == 1 && linebuf[0] == '\n')
				continue;
			strncat(buf, linebuf, sizeof(buf));
			buf[sizeof(buf) - 1] = '\0';
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
	int yanked;
	if(count > 0)
		get_selected_files(view, count, indexes);
	else
		get_all_selected_files(curr_view);

	yank_selected_files(curr_view, reg);
	yanked = curr_view->selected_files;
	free_selected_file_array(curr_view);

	if(count == 0)
	{
		clean_selected_files(curr_view);
		draw_dir_list(curr_view, curr_view->top_line);
		moveto_list_pos(curr_view, curr_view->list_pos);
		count = yanked;
	}

	status_bar_messagef(" %d %s yanked.", yanked, yanked == 1 ? "file" : "files");

	return yanked;
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

	args[0] = cfg.shell;
	args[1] = "-c";
	args[2] = command;
	args[3] = NULL;

	pid = execute(args);
	return pid;
}

void
view_file(const char *filename)
{
	char command[PATH_MAX + 5] = "";
	char *escaped;

	if(access(filename, F_OK) != 0)
	{
		show_error_msg("Broken Link", "Link destination doesn't exist");
		return;
	}

	escaped = escape_filename(filename, 0, 0);
	snprintf(command, sizeof(command), "%s %s", get_vicmd(), escaped);
	free(escaped);

	shellout(command, -1);
	curs_set(0);
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

/* Returns pointer to a statically allocated buffer */
static const char *
make_name_unique(const char *filename)
{
	static char unique[PATH_MAX];
	size_t len;
	int i;

	len = snprintf(unique, sizeof(unique), "%s_%u%u_00", filename, getppid(),
			getpid());
	i = 0;

	while(access(unique, F_OK) == 0)
		sprintf(unique + len - 2, "%d", ++i);
	return unique;
}

/*
 * mount_point should be an array of at least PATH_MAX characters
 * Returns 0 on success.
 */
static int
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

	escaped_filename = escape_filename(get_current_file_name(view), 0, 0);

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
	if(mkdir(mount_point, S_IRWXU))
	{
		free(escaped_filename);
		show_error_msg("Unable to create FUSE mount directory", mount_point);
		return -1;
	}
	free(escaped_filename);

	escaped_path = escape_filename(filename, 0, 0);
	escaped_mount_point = escape_filename(mount_point, 0, 0);

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
		show_error_msg("FUSE MOUNT ERROR", "Can't chdir() to FUSE home");
		return -1;
	}

	if(clear_before_mount)
	{
		def_prog_mode();
		endwin();
	}

	tmp_file = make_name_unique("/tmp/vifm.errors");
	strcat(buf, " 2> ");
	strcat(buf, tmp_file);
	int status = background_and_wait_for_status(buf);
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
		show_error_msg("FUSE MOUNT ERROR", filename);
		(void)chdir(view->curr_dir);
		return -1;
	}
	unlink(tmp_file);
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

	return 0;
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

			if(fgets(filename, sizeof(filename), f) == NULL)
			{
				status_bar_message("Can't read file content");
				curr_stats.save_msg = 1;
				fclose(f);
				return;
			}
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
		if(fuse_mount(view, filename, program, mount_point) != 0)
			return;
	}

	change_directory(view, mount_point);
	load_dir_list(view, 0);
	moveto_list_pos(view, view->curr_line);
}

static void
execute_file(FileView *view, int dont_execute)
{
	char *program = NULL;
	int undef;
	int same;
	int i;

	if(!view->dir_entry[view->list_pos].selected)
		clean_selected_files(view);

	program = get_default_program_for_file(view->dir_entry[view->list_pos].name);
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

	if(!same && undef == 0)
	{
		free(program);
		show_error_msg("Selection error", "Files have different programs");
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
		if(view->selected_files <= 1)
			view_file(get_current_file_name(view));
		else
		{
			program = edit_selection(view);
			shellout(program, -1);
			free(program);
		}
		return;
	}

	run_using_prog(view, program, dont_execute, 0);
	free(program);
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
		show_error_msg("Broken Link", "Link destination doesn't exist");
		return;
	}

	if(strncmp(program, "FUSE_MOUNT", 10) == 0
			|| strncmp(program, "FUSE_MOUNT2", 11) == 0)
	{
		if(dont_execute)
			view_file(get_current_file_name(view));
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
		char *temp = escape_filename(view->dir_entry[view->list_pos].name, 0, 0);

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
	ssize_t len;
	char *filename;

	filename = strdup(view->dir_entry[view->list_pos].name);
	chosp(filename);

	len = readlink(filename, linkto, sizeof(linkto));

	free(filename);

	if(len == -1)
	{
		status_bar_message("Couldn't Resolve Link");
		curr_stats.save_msg = 1;
		return;
	}

	linkto[len] = '\0';

	if(access(linkto, F_OK) != 0)
	{
		status_bar_message("Can't access link destination. It might be broken");
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
		change_directory(view, dir);
		load_dir_list(view, 0);
		moveto_list_pos(view, view->curr_line);
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
			moveto_list_pos(view, pos);
	}
	free(link_dup);
	free(dir);
}

void
handle_file(FileView *view, int dont_execute, int force_follow)
{
	int type;
	int runnable;
	int run_link;
	char *filename;
	
	filename = get_current_file_name(view);
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
			show_error_msg("Selection error",
					"Selection cannot contain files and directories at the same time");
			return;
		}
	}

	if(type == DIRECTORY && view->selected_files == 0)
	{
		if(strcmp(filename, "../") == 0)
		{
			cd_updir(view);
		}
		else if(change_directory(view, filename) == 0)
		{
			load_dir_list(view, 0);
			moveto_list_pos(view, view->curr_line);
		}
		return;
	}

	if(cfg.vim_filter)
		use_vim_plugin(view, 0, NULL); /* no return */

	run_link = !cfg.follow_links && type == LINK && !check_link_is_dir(filename);
	if(run_link && force_follow)
		run_link = 0;
	if(view->selected_files > 0)
		run_link = 1;

	runnable = type == EXECUTABLE || (run_link && access(filename, X_OK) == 0);

	if(runnable && !dont_execute && cfg.auto_execute)
	{
		char buf[NAME_MAX];
		snprintf(buf, sizeof(buf), "./%s", filename);
		shellout(buf, 1);
	}
	else if(type == REGULAR || type == EXECUTABLE || run_link ||
			type == DIRECTORY)
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
is_dir_writable(const char *path)
{
	if(access(path, W_OK) == 0)
		return 1;
	show_error_msg("Operation error", "Current directory is not writable");
	return 0;
}

/* returns new value for save_msg */
int
delete_file(FileView *view, int reg, int count, int *indexes, int use_trash)
{
	char buf[8 + PATH_MAX*2];
	char undo_buf[8 + PATH_MAX*2];
	int x, y;
	size_t len;

	if(!is_dir_writable(view->curr_dir))
		return 0;

	if(cfg.use_trash && use_trash &&
			path_starts_with(view->curr_dir, cfg.trash_dir))
	{
		status_bar_message("Can't perform deletion. "
				"Current directory is under Trash directory");
		return 1;
	}

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
	len = strlen(buf);

	for(x = 0; x < view->selected_files && len < COMMAND_GROUP_INFO_LEN; x++)
	{
		if(buf[len - 2] != ':')
		{
			strncat(buf, ", ", sizeof(buf));
			buf[sizeof(buf) - 1] = '\0';
		}
		strncat(buf, view->selected_filelist[x], sizeof(buf));
		buf[sizeof(buf) - 1] = '\0';
		len = strlen(buf);
	}
	cmd_group_begin(buf);

	y = 0;
	if(chdir(curr_view->curr_dir) != 0)
	{
		status_bar_message("Can't chdir() to current directory");
		return 1;
	}
	for(x = 0; x < view->selected_files; x++)
	{
		char *esc_file, *esc_full, *esc_dest, *dest;
		char full_buf[PATH_MAX];

		if(strcmp("../", view->selected_filelist[x]) == 0)
		{
			show_error_msg("Background Process Error",
					"You cannot delete the ../ directory");
			continue;
		}

		if(check_link_is_dir(view->selected_filelist[x]))
			chosp(view->selected_filelist[x]);

		snprintf(full_buf, sizeof(full_buf), "%s/%s", view->curr_dir,
				view->selected_filelist[x]);
		esc_full = escape_filename(full_buf, 0, 0);
		esc_file = escape_filename(view->selected_filelist[x], 0, 0);
		dest = gen_trash_name(view->selected_filelist[x]);
		esc_dest = escape_filename(dest, 0, 0);
		if(cfg.use_trash && use_trash)
		{
			snprintf(buf, sizeof(buf), "mv -n %s %s", esc_full, esc_dest);
			snprintf(undo_buf, sizeof(undo_buf), "mv -n %s %s", esc_dest, esc_full);
		}
		else
		{
			snprintf(buf, sizeof(buf), "rm -rf %s", esc_full);
			undo_buf[0] = '\0';
		}
		free(esc_dest);
		free(esc_file);
		free(esc_full);

		progress_msg("Deleting files", x + 1, view->selected_files);
		if(background_and_wait_for_errors(buf) == 0)
		{
			if(cfg.use_trash && use_trash)
			{
				add_operation(buf, full_buf, dest, undo_buf, dest, full_buf);
				append_to_register(reg, dest);
			}
			else
			{
				add_operation(buf, full_buf, NULL, undo_buf, NULL, NULL);
			}
			y++;
		}
		free(dest);
	}
	free_selected_file_array(view);

	cmd_group_end();

	get_all_selected_files(view);
	load_dir_list(view, 1);
	free_selected_file_array(view);

	/* some files may still exist if there was an error */
	view->selected_files = 0;
	for(x = 0; x < view->list_rows; x++)
		view->selected_files += view->dir_entry[x].selected;

	moveto_list_pos(view, view->list_pos);

	status_bar_messagef("%d %s deleted", y, y == 1 ? "file" : "files");
	return 1;
}

static int
mv_file(const char *src, const char *dst, int tmpfile_num)
{
	char full_src[PATH_MAX], full_dst[PATH_MAX];
	char do_command[6 + PATH_MAX*2 + 1];
	char undo_command[6 + PATH_MAX*2 + 1];
	char *escaped_src, *escaped_dst;
	int result;

	snprintf(full_src, sizeof(full_src), "%s/%s", curr_view->curr_dir, src);
	chosp(full_src);
	escaped_src = escape_filename(full_src, 0, 0);
	snprintf(full_dst, sizeof(full_dst), "%s/%s", curr_view->curr_dir, dst);
	chosp(full_dst);
	escaped_dst = escape_filename(full_dst, 0, 0);

	if(escaped_src == NULL || escaped_dst == NULL)
	{
		free(escaped_src);
		free(escaped_dst);
		return -1;
	}

	snprintf(do_command, sizeof(do_command), "mv -n %s %s", escaped_src,
			escaped_dst);
	snprintf(undo_command, sizeof(do_command), "mv -n %s %s", escaped_dst,
			escaped_src);
	free(escaped_src);
	free(escaped_dst);

	result = system_and_wait_for_errors(do_command);
	if(result == 0)
	{
		if(tmpfile_num == 0)
			add_operation(do_command, full_src, full_dst, undo_command, full_dst,
					full_src);
		else if(tmpfile_num == 1)
			add_operation(do_command, full_dst, NULL, undo_command, full_dst, NULL);
		else if(tmpfile_num == 2)
			add_operation(do_command, full_src, NULL, undo_command, full_src, NULL);
	}
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
		status_bar_message("Name can not contain slash");
		curr_stats.save_msg = 1;
		return;
	}

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
		return;
	}

	snprintf(buf, sizeof(buf), "rename in %s: %s to %s",
			replace_home_part(curr_view->curr_dir), filename, new);
	cmd_group_begin(buf);
	tmp = mv_file(filename, new, 0);
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

	clean_selected_files(curr_view);
	draw_dir_list(curr_view, curr_view->top_line);
	moveto_list_pos(curr_view, curr_view->list_pos);

	strncpy(buf, get_current_file_name(view), sizeof(buf));
	buf[sizeof(buf) - 1] = '\0';
	if(strcmp(buf, "../") == 0)
	{
		status_bar_message("You can't rename parent directory this way.");
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

	enter_prompt_mode(L"New name: ", buf, rename_file_cb);
}

/* Returns not NULL if file is OK */
static char **
check_rename_file(FileView *view, int *indexes, int count, FILE *f)
{
	int i;
	char **list = NULL;
	size_t len = 0;

	for(i = 0; i < count; i++)
	{
		int j;
		char name[NAME_MAX];

		if(fgets(name, sizeof(name), f) == NULL)
		{
			status_bar_message("Not enough lines");
			curr_stats.save_msg = 1;
			free_string_array(list, len);
			return NULL;
		}
		chomp(name);

		if(strchr(name, '/') != NULL)
		{
			status_bar_message("File name can not contain slash");
			curr_stats.save_msg = 1;
			free_string_array(list, len);
			return NULL;
		}

		if(name[0] != '\0')
		{
			for(j = 0; j < len; j++)
				if(strcmp(name, list[j]) == 0)
				{
					status_bar_message("There are duplicates");
					curr_stats.save_msg = 1;
					free_string_array(list, len);
					return NULL;
				}
		}

		list = realloc(list, sizeof(char*)*(len + 1));
		list[len] = strdup(name);
		len++;

		if(name[0] == '\0')
			continue;

		if(strcmp(name, view->dir_entry[abs(indexes[i])].name) == 0)
			continue;

		for(j = 0; j < count; j++)
		{
			chosp(view->dir_entry[abs(indexes[j])].name);
			if(strcmp(name, view->dir_entry[abs(indexes[j])].name) == 0 &&
					indexes[j] >= 0)
			{
				indexes[j] = -indexes[j];
				break;
			}
		}
		if(j >= count && access(name, F_OK) == 0)
		{
			status_bar_messagef("File %s already exists", name);
			curr_stats.save_msg = 1;
			free_string_array(list, len);
			return NULL;
		}
	}

	if(fgetc(f) != EOF)
	{
		status_bar_message("Too many lines");
		curr_stats.save_msg = 1;
		free_string_array(list, len);
		return NULL;
	}

	return list;
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
		if(mv_file(view->dir_entry[ind].name, tmp, 2) != 0)
		{
			cmd_group_end();
			undo_group();
			status_bar_message("Temporary rename error");
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

		if(mv_file(view->dir_entry[abs(indexes[i])].name, list[i], indexes[i] < 0)
				== 0)
			renamed++;
	}

	cmd_group_end();

	return renamed;
}

static void
rename_files_ind(FileView *view, int *indexes, int count)
{
	char temp_file[PATH_MAX];
	char **list;
	struct stat st_before, st_after;
	FILE *f;
	int i, renamed = 0;

	if(count == 0)
	{
		status_bar_message("0 files renamed.");
		return;
	}

	strncpy(temp_file, make_name_unique("vifm-rename"), sizeof(temp_file));

	if((f = fopen(temp_file, "w")) == NULL)
	{
		status_bar_message("Can't create temp file.");
		return;
	}

	for(i = 0; i < count; i++)
	{
		char *name = view->dir_entry[indexes[i]].name;
		size_t len = strlen(name);
		int slash = name[len - 1] == '/';
		if(slash)
			name[len - 1] = '\0';
		fprintf(f, "%s\n", name);
		if(slash)
			name[len - 1] = '/';
	}

	fclose(f);

	stat(temp_file, &st_before);

	view_file(temp_file);

	stat(temp_file, &st_after);

	if(memcmp(&st_after.st_mtim, &st_before.st_mtim,
			sizeof(st_after.st_mtime)) == 0)
	{
		unlink(temp_file);
		status_bar_message("0 files renamed.");
		return;
	}

	if((f = fopen(temp_file, "r")) == NULL)
	{
		unlink(temp_file);
		status_bar_message("Can't open temporary file.");
		return;
	}

	if((list = check_rename_file(view, indexes, count, f)) != NULL)
		renamed = perform_renaming(view, indexes, count, list);

	fclose(f);

	unlink(temp_file);

	if(list != NULL)
	{
		status_bar_messagef("%d file%s renamed.", renamed,
				(renamed == 1) ? "" : "s");

		free_string_array(list, count);
	}

	load_dir_list(view, 1);
	moveto_list_pos(view, view->list_pos);
}

void
rename_files(FileView *view)
{
	int *indexes;
	int count;
	int i, j;

	if(view->selected_files == 0)
	{
		view->dir_entry[view->list_pos].selected = 1;
		view->selected_files = 1;
	}

	count = view->selected_files;
	indexes = malloc(sizeof(*indexes)*count);
	if(indexes == NULL)
	{
		status_bar_message("Not enough memory.");
		return;
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

	rename_files_ind(view, indexes, count);

	free(indexes);

	clean_selected_files(curr_view);
	draw_dir_list(curr_view, curr_view->top_line);
	moveto_list_pos(curr_view, curr_view->list_pos);
	curr_stats.save_msg = 1;
}

static void
change_owner_cb(const char *new_owner)
{
	char *filename;
	char buf[8 + NAME_MAX + 1];
	char full[PATH_MAX];
	char command[10 + 32 + PATH_MAX];
	char undo_command[10 + 32 + PATH_MAX];
	char *escaped;

	if(new_owner == NULL || new_owner[0] == '\0')
		return;

	filename = get_current_file_name(curr_view);
	snprintf(full, sizeof(full), "%s/%s", curr_view->curr_dir, filename);
	escaped = escape_filename(full, 0, 0);
	snprintf(command, sizeof(command), "chown -fR %s %s", new_owner, escaped);
	snprintf(undo_command, sizeof(undo_command), "chown -fR %d %s",
			curr_view->dir_entry[curr_view->list_pos].gid, escaped);
	free(escaped);

	if(system_and_wait_for_errors(command) != 0)
		return;

	snprintf(buf, sizeof(buf), "chown in %s: %s",
			replace_home_part(curr_view->curr_dir), filename);
	cmd_group_begin(buf);
	add_operation(command, full, NULL, undo_command, full, NULL);
	cmd_group_end();

	load_dir_list(curr_view, 1);
	moveto_list_pos(curr_view, curr_view->list_pos);
}

void
change_owner(void)
{
	enter_prompt_mode(L"New owner: ", "", change_owner_cb);
}

static void
change_group_cb(const char *new_group)
{
	char *filename;
	char buf[8 + NAME_MAX + 1];
	char full[PATH_MAX];
	char command[10 + 32 + PATH_MAX];
	char undo_command[10 + 32 + PATH_MAX];
	char *escaped;

	if(new_group == NULL || new_group[0] == '\0')
		return;

	filename = get_current_file_name(curr_view);
	snprintf(full, sizeof(full), "%s/%s", curr_view->curr_dir, filename);
	escaped = escape_filename(full, 0, 0);
	snprintf(command, sizeof(command), "chown -fR :%s %s", new_group, escaped);
	snprintf(undo_command, sizeof(undo_command), "chown -fR :%d %s",
			curr_view->dir_entry[curr_view->list_pos].uid, escaped);
	free(escaped);

	if(system_and_wait_for_errors(command) != 0)
		return;

	snprintf(buf, sizeof(buf), "chgrp in %s: %s",
			replace_home_part(curr_view->curr_dir), filename);
	cmd_group_begin(buf);
	add_operation(command, full, NULL, undo_command, full, NULL);
	cmd_group_end();

	load_dir_list(curr_view, 1);
	moveto_list_pos(curr_view, curr_view->list_pos);
}

void
change_group(void)
{
	enter_prompt_mode(L"New group: ", "", change_group_cb);
}

static void
prompt_dest_name(const char *src_name)
{
	wchar_t buf[256];

	swprintf(buf, ARRAY_LEN(buf), L"New name for %s: ", src_name);
	enter_prompt_mode(buf, src_name, put_confirm_cb);
}

static void
prompt_what_to_do(const char *src_name)
{
	wchar_t buf[NAME_MAX];

	free(put_confirm.name);
	put_confirm.name = strdup(src_name);
	swprintf(buf, sizeof(buf)/sizeof(buf[0]),
			L"Name conflict for %s. [r]ename/[s]kip/[o]verwrite/overwrite [a]ll: ",
			src_name);
	enter_prompt_mode(buf, "", put_decide_cb);
}

/* Returns 0 on success */
static int
put_next_file(const char *dest_name, int override)
{
	char *filename;
	char *src_buf, *dst_buf, *name_buf = NULL;
	struct stat st;

	override = override || put_confirm.overwrite_all;

	filename = put_confirm.reg->files[put_confirm.x];
	chosp(filename);
	src_buf = escape_filename(filename, 0, 0);
	dst_buf = escape_filename(put_confirm.view->curr_dir, 0, 0);
	if(lstat(filename, &st) == 0 && src_buf != NULL && dst_buf != NULL)
	{
		char do_buf[PATH_MAX + NAME_MAX*2 + 4];
		char undo_buf[PATH_MAX + NAME_MAX*2 + 4];
		const char *p = dest_name;
		int from_trash = strncmp(filename, cfg.trash_dir,
				strlen(cfg.trash_dir)) == 0;
		int move = from_trash || put_confirm.force_move;

		if(p[0] == '\0')
			p = strrchr(filename, '/') + 1;

		if(from_trash)
		{
			while(isdigit(*p))
				p++;
			p++;
		}

		if(access(p, F_OK) == 0 && !override)
		{
			free(src_buf);
			free(dst_buf);
			prompt_what_to_do(p);
			return 1;
		}

		if(dest_name[0] == '\0')
		{
			name_buf = escape_filename(p, 0, 0);
			dest_name = name_buf;
		}

		if(override)
		{
			if(access(p, F_OK) == 0)
			{
				snprintf(do_buf, sizeof(do_buf), "rm -rf %s/%s", dst_buf, dest_name);
				if(background_and_wait_for_errors(do_buf) != 0)
				{
					free(src_buf);
					free(dst_buf);
					free(name_buf);
					return 0;
				}
			}

			put_confirm.view->dir_mtime = 0;
		}

		if(move)
		{
			snprintf(do_buf, sizeof(do_buf), "mv %s %s %s/%s", override ? "" : "-n",
					src_buf, dst_buf, dest_name);
			snprintf(undo_buf, sizeof(undo_buf), "mv -n %s/%s %s", dst_buf, dest_name,
					src_buf);
		}
		else
		{
			snprintf(do_buf, sizeof(do_buf), "cp %s -pR %s %s/%s",
					override ? "" : "-n", src_buf, dst_buf, dest_name);
			snprintf(undo_buf, sizeof(undo_buf), "rm -rf %s/%s", dst_buf, dest_name);
		}

		progress_msg("Putting files", put_confirm.x + 1,
				put_confirm.reg->num_files);
		if(background_and_wait_for_errors(do_buf) == 0)
		{
			char *msg;
			size_t len;
			char dst_full[PATH_MAX];

			snprintf(dst_full, sizeof(dst_full), "%s/%s", put_confirm.view->curr_dir,
					p);
			cmd_group_continue();

			msg = replace_group_msg(NULL);
			len = strlen(msg);
			msg = realloc(msg, COMMAND_GROUP_INFO_LEN);
			
			snprintf(msg + len, COMMAND_GROUP_INFO_LEN - len, "%s%s",
					(msg[len - 2] != ':') ? ", " : "", p);
			replace_group_msg(msg);

			if(move)
				add_operation(do_buf, filename, dst_full, undo_buf, dst_full, filename);
			else
				add_operation(do_buf, filename, dst_full, undo_buf, dst_full, NULL);

			cmd_group_end();
			put_confirm.y++;
			if(move)
			{
				free(put_confirm.reg->files[put_confirm.x]);
				put_confirm.reg->files[put_confirm.x] = NULL;
			}
		}
	}
	free(src_buf);
	free(dst_buf);
	free(name_buf);
	return 0;
}

static void
put_confirm_cb(const char *dest_name)
{
	if(dest_name == NULL || dest_name[0] == '\0')
		return;

	if(put_next_file(dest_name, 0) == 0)
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
		if(put_next_file("", 1) == 0)
		{
			put_confirm.x++;
			curr_stats.save_msg = put_files_from_register_i(put_confirm.view, 0);
		}
	}
	else if(strcmp(choice, "a") == 0)
	{
		put_confirm.overwrite_all = 1;
		if(put_next_file("", 1) == 0)
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
		int from_trash = strncmp(put_confirm.reg->files[0], cfg.trash_dir,
				strlen(cfg.trash_dir)) == 0;
		snprintf(buf, sizeof(buf), "%s in %s: ",
				(put_confirm.force_move || from_trash) ?  "Put" : "put",
				replace_home_part(view->curr_dir));
		cmd_group_begin(buf);
		cmd_group_end();
	}

	if(chdir(view->curr_dir) != 0)
	{
		status_bar_message("Can't chdir() to current directory");
		return 1;
	}
	while(put_confirm.x < put_confirm.reg->num_files)
	{
		if(put_next_file("", 0) != 0)
			return 0;
		put_confirm.x++;
	}

	pack_register(put_confirm.reg->name);

	status_bar_messagef(" %d file%s inserted", put_confirm.y,
			(put_confirm.y == 1) ? "" : "s");

	return 1;
}

/* Returns new value for save_msg flag. */
int
put_files_from_register(FileView *view, int name, int force_move)
{
	registers_t *reg;

	if(!is_dir_writable(view->curr_dir))
		return 0;

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
	put_confirm.overwrite_all = 0;
	return put_files_from_register_i(view, 1);
}

static void
clone_file(FileView* view, const char *filename)
{
	char do_cmd[PATH_MAX + NAME_MAX*2 + 4];
	char undo_cmd[3 + PATH_MAX + 6 + 1];
	char clone_name[PATH_MAX];
	char *escaped, *escaped_clone;
	int i;
	size_t len;
	
	if(strcmp(filename, "./") == 0)
		return;
	if(strcmp(filename, "../") == 0)
		return;

	snprintf(clone_name, sizeof(clone_name), "%s", filename);
	chosp(clone_name);
	i = 1;
	len = strlen(clone_name);
	do
		snprintf(clone_name + len, sizeof(clone_name) - len, "(%d)", i++);
	while(access(clone_name, F_OK) == 0);

	snprintf(do_cmd, sizeof(do_cmd), "%s/%s", view->curr_dir, filename);
	escaped = escape_filename(do_cmd, 0, 0);
	chosp(escaped);
	escaped_clone = escape_filename(clone_name, 0, 0);
	snprintf(do_cmd, sizeof(do_cmd), "cp -npR %s %s", escaped, escaped_clone);
	snprintf(undo_cmd, sizeof(undo_cmd), "rm -rf %s", escaped_clone);
	free(escaped_clone);
	free(escaped);

	if(background_and_wait_for_errors(do_cmd) == 0)
		add_operation(do_cmd, filename, clone_name, undo_cmd, clone_name, NULL);
}

/* returns new value for save_msg */
int
clone_files(FileView *view)
{
	size_t len;
	int i;
	char buf[COMMAND_GROUP_INFO_LEN + 1];

	if(!is_dir_writable(view->curr_dir))
		return 0;

	if(view->selected_files == 0)
	{
		view->dir_entry[view->list_pos].selected = 1;
		view->selected_files = 1;
	}
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
			moveto_list_pos(view, view->list_pos);
			return 0;
		}
	}
	get_all_selected_files(view);

	len = snprintf(buf, sizeof(buf), "clone in %s: ", view->curr_dir);
	for(i = 0; i < view->selected_files && len < COMMAND_GROUP_INFO_LEN; i++)
	{
		if(buf[len - 2] != ':')
		{
			strncat(buf, ", ", sizeof(buf));
			buf[sizeof(buf) - 1] = '\0';
		}
		strncat(buf, view->selected_filelist[i], sizeof(buf));
		buf[sizeof(buf) - 1] = '\0';
		len = strlen(buf);
	}

	cmd_group_begin(buf);
	for(i = 0; i < view->selected_files; i++)
		clone_file(view, view->selected_filelist[i]);
	cmd_group_end();
	free_selected_file_array(view);

	clean_selected_files(view);
	load_saving_pos(view, 1);
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
		if(dentry->d_type == DT_DIR)
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
