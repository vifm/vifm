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


#include<ncurses.h>
#include<unistd.h>
#include<errno.h> /* errno */
#include<sys/wait.h> /* waitpid() */
#include<sys/types.h> /* waitpid() */
#include<sys/stat.h> /* stat */
#include<stdio.h>
#include<string.h>

#include"background.h"
#include"color_scheme.h"
#include"commands.h"
#include"config.h"
#include"filelist.h"
#include"fileops.h"
#include"filetype.h"
#include"keys.h"
#include"menus.h"
#include"registers.h"
#include"status.h"
#include"ui.h"
#include"utils.h"



int
my_system(char *command)
{
	int pid;
	int status;
	extern char **environ;

	if(command == 0)
		return 1;

	pid = fork();
	if(pid == -1)
		return -1;
	if(pid == 0)
	{
		char *args[4];

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

void
yank_selected_files(FileView *view)
{
	int x;
	size_t namelen;
	int old_list = curr_stats.num_yanked_files;



	if(curr_stats.yanked_files)
	{
		for(x = 0; x < old_list; x++)
		{
			free(curr_stats.yanked_files[x]);
		}
		free(curr_stats.yanked_files);
		curr_stats.yanked_files = NULL;
	}

	curr_stats.yanked_files = (char **)calloc(view->selected_files,
			sizeof(char *));

	if ((curr_stats.use_register) && (curr_stats.register_saved))
	{
		/* A - Z  append to register otherwise replace */
		if ((curr_stats.curr_register < 65) || (curr_stats.curr_register > 90))
			clear_register(curr_stats.curr_register);
		else
			curr_stats.curr_register = curr_stats.curr_register + 32;
	}

	for(x = 0; x < view->selected_files; x++)
	{
		if(view->selected_filelist[x])
		{
			char buf[PATH_MAX];
			namelen = strlen(view->selected_filelist[x]);
			curr_stats.yanked_files[x] = malloc(namelen +1);
			strcpy(curr_stats.yanked_files[x], view->selected_filelist[x]);
			snprintf(buf, sizeof(buf), "%s/%s", view->curr_dir,
					view->selected_filelist[x]);
			append_to_register(curr_stats.curr_register, view->selected_filelist[x]);
		}
		else
		{
			x--;
			break;
		}
	}
	curr_stats.num_yanked_files = x;

	strncpy(curr_stats.yanked_files_dir, view->curr_dir,
			sizeof(curr_stats.yanked_files_dir) -1);
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
		lstat(linkto, &s);

		if((s.st_mode & S_IFMT) == S_IFDIR)
			return 1;
	}


	return 0;
}

void
handle_file(FileView *view, int dont_execute)
{
	if(DIRECTORY == view->dir_entry[view->list_pos].type)
	{
		char *filename = get_current_file_name(view);
		change_directory(view, filename);
		load_dir_list(view, 0);
		moveto_list_pos(view, view->curr_line);
		return;
	}

	if(cfg.vim_filter)
	{
		FILE *fp;
		char filename[PATH_MAX] = "";

		snprintf(filename, sizeof(filename), "%s/vimfiles", cfg.config_dir);
		fp = fopen(filename, "w");
		snprintf(filename, sizeof(filename), "%s/%s",
				view->curr_dir,
				view->dir_entry[view->list_pos].name);
		endwin();
		fprintf(fp, "%s", filename);
		fclose(fp);
		exit(0);
	}

	if(EXECUTABLE == view->dir_entry[view->list_pos].type && !dont_execute)
	{
		if(cfg.auto_execute)
		{
			char buf[NAME_MAX];
			snprintf(buf, sizeof(buf), "./%s", get_current_file_name(view));
			shellout(buf, 1);
			return;
		}
		else /* Check for a filetype */
		{
			char *program = NULL;

			if((program = get_default_program_for_file(
						view->dir_entry[view->list_pos].name)) != NULL)
			{
				if(strchr(program, '%'))
				{
					int m = 0;
					int s = 0;
					char *command = expand_macros(view, program, NULL, &m, &s);
					shellout(command, 0);
					free(command);
					return;
				}
				else
				{
					char buf[NAME_MAX *2];
					char *temp = escape_filename(view->dir_entry[view->list_pos].name,
						   strlen(view->dir_entry[view->list_pos].name), 0);

					snprintf(buf, sizeof(buf), "%s %s", program, temp);
					shellout(buf, 0);
					free(program);
					free(temp);
					return;
				}
			}
			else /* vi is set as the default for any extension without a program */
			{
				view_file(view);
			}
			return;
		}
	}
	if((REGULAR == view->dir_entry[view->list_pos].type)
				|| (EXECUTABLE == view->dir_entry[view->list_pos].type))
	{
		char *program = NULL;

		if((program = get_default_program_for_file(
					view->dir_entry[view->list_pos].name)) != NULL)
		{
/*_SZ_BEGIN_*/
			if(!strncmp(program, "FUSE_MOUNT", 10))
			{
				if(access(cfg.fuse_home, F_OK|W_OK|X_OK) != 0)
				{
					if(mkdir(cfg.fuse_home, S_IRWXU))
					{
						show_error_msg("Unable to create FUSE mount home directory", (char *)cfg.fuse_home);
						return;
					}
								}
				Fuse_List *runner = fuse_mounts, *fuse_item = NULL;
				char filename[PATH_MAX];
				char mount_point[PATH_MAX];
				int mount_point_id = 0, mount_found = 0;
				snprintf(filename, PATH_MAX, "%s/%s",
					view->curr_dir,
					get_current_file_name(view)
				);
				/*check if already mounted*/
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
				/*new file to be mounted*/
				if(!mount_found)
				{
					/*get mount_point_id + mount_point and set runner pointing to the list's tail*/
					if(fuse_mounts)
					{
						runner = fuse_mounts;
						while(runner->next != NULL)
							runner = runner->next;
						mount_point_id = runner->mount_point_id + 1;
					}
					snprintf(mount_point, PATH_MAX, "%s/%03d_%s",
						cfg.fuse_home,
						mount_point_id,
						get_current_file_name(view)
					);
					if(mkdir(mount_point, S_IRWXU))
					{
						show_error_msg("Unable to create FUSE mount directory", mount_point);
						return;
					}
					/*I need the return code of the mount command*/
					status_bar_message("FUSE mounting selected file, please stand by..");
					char buf[2*PATH_MAX];
					char cmd_buf[96];
					char *cmd_pos;
					char *buf_pos = buf;
					char *prog_pos = program;
/*Build the mount command based on the FUSE program config line in vifmrc.
  Accepted FORMAT: FUSE_MOUNT|some_mount_command %SOURCE_FILE %DESTINATION_DIR*/
					strcpy(buf_pos, "sh -c \"");
					//strcpy(buf_pos, "sh -c \"pauseme PAUSE_ON_ERROR_ONLY ");
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
							if(!strcmp(cmd_buf, "%SOURCE_FILE") && (buf_pos+strlen(filename)<buf+sizeof(buf)+2))
							{
								*buf_pos++='\'';
								strcpy(buf_pos, filename);
								buf_pos += strlen(filename);
								*buf_pos++='\'';
							}
							else if(!strcmp(cmd_buf, "%DESTINATION_DIR") && (buf_pos+strlen(filename)<buf+sizeof(buf)+2))
							{
								*buf_pos++='\'';
								strcpy(buf_pos, mount_point);
								buf_pos += strlen(mount_point);
								*buf_pos++='\'';
							}
						}
						else
						{
							*buf_pos = *prog_pos;
							if(buf_pos < buf_pos+sizeof(buf)-1)
								buf_pos++;
							prog_pos++;
						}
					}

					*buf_pos = '"';
					*(++buf_pos) = '\0';
					/*uff, CMD built.*/
					/*Just before running the mount,
					  I need to chdir out temporarily from any FUSE mounted
					  paths, Otherwise the fuse-zip command fails with
					  "fusermount: failed to open current
						directory: permission denied"
					 *(this happens when mounting JARs from mounted JARs)*/
					chdir(cfg.fuse_home);
					/*
					def_prog_mode();
					endwin();
					my_system("clear");
					int status = my_system(buf);
					*/
					int status = background_and_wait_for_status(buf);
					/*check child status*/
					if( !WIFEXITED(status) || (WIFEXITED(status) &&
								WEXITSTATUS(status)) )
					{
						werase(status_bar);
						/*remove the DIR we created for the mount*/
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
				change_directory(view, mount_point);
				load_dir_list(view, 0);
				moveto_list_pos(view, view->curr_line);
				return;
			}
			/*_SZ_END_*/
			if(strchr(program, '%'))
			{
				int m = 0;
				int s = 0;
				char *command = expand_macros(view, program, NULL, &m, &s);
				shellout(command, 0);
				free(command);
				return;
			}
			else
			{
				char buf[NAME_MAX *2];
				char *temp = escape_filename(view->dir_entry[view->list_pos].name,
						strlen(view->dir_entry[view->list_pos].name), 0);

				snprintf(buf, sizeof(buf), "%s %s", program, temp);
				shellout(buf, 0);
				free(program);
				free(temp);
				return;
			}
		}
		else /* vi is set as the default for any extension without a program */
			view_file(view);

		return;
	}
	if(LINK == view->dir_entry[view->list_pos].type)
	{
		char linkto[PATH_MAX +NAME_MAX];
		int len;
		char *filename = strdup(view->dir_entry[view->list_pos].name);
		len = strlen(filename);
		if (filename[len - 1] == '/')
			filename[len - 1] = '\0';

		len = readlink (filename, linkto, sizeof (linkto));

		free(filename);

		if (len > 0)
		{
			struct stat s;
			int is_dir = 0;
			int is_file = 0;
			char *dir = NULL;
			char *file = NULL;
			char *link_dup = strdup(linkto);
			linkto[len] = '\0';
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
				if((file = strrchr(link_dup, '/')))
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
		else
			status_bar_message("Couldn't Resolve Link");
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

  if (pipe (file_pipes) != 0)
	  return 1;

  if ((pid = fork ()) == -1)
	  return 1;

  if (pid == 0)
	{
			close(1);
			close(2);
			dup(file_pipes[1]);
	  close (file_pipes[0]);
	  close (file_pipes[1]);

	  args[0] = "sh";
	  args[1] = "-c";
	  args[2] = command;
	  args[3] = NULL;
	  execvp (args[0], args);
	  exit (127);
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


void
delete_file(FileView *view)
{
	char buf[256];
	int x;

	if(!view->selected_files)
	{
		view->dir_entry[view->list_pos].selected = 1;
		view->selected_files = 1;
	}

	get_all_selected_files(view);
	yank_selected_files(view);

	for(x = 0; x < view->selected_files; x++)
	{
		if(!strcmp("../", view->selected_filelist[x]))
		{
			show_error_msg(" Background Process Error ",
					"You cannot delete the ../ directory ");
			continue;
		}

		if ((curr_stats.use_register) && (curr_stats.register_saved))
		{
			strncpy(curr_stats.yanked_files_dir, cfg.trash_dir,
					sizeof(curr_stats.yanked_files_dir) -1);
			snprintf(buf, sizeof(buf), "mv \"%s\" %s/%s",
					view->selected_filelist[x], cfg.trash_dir,
					view->selected_filelist[x]);

			curr_stats.register_saved = 0;
			curr_stats.use_register = 0;
		}
		else if(cfg.use_trash)
		{
			strncpy(curr_stats.yanked_files_dir, cfg.trash_dir,
					sizeof(curr_stats.yanked_files_dir) -1);
			snprintf(buf, sizeof(buf), "mv \"%s\" %s",
				view->selected_filelist[x],  cfg.trash_dir);
		}
		else
			snprintf(buf, sizeof(buf), "rm -fr '%s'",
					 view->selected_filelist[x]);

		background_and_wait_for_errors(buf);
	}
	free_selected_file_array(view);
	view->selected_files = 0;

	load_dir_list(view, 1);

	moveto_list_pos(view, view->list_pos);
}

void
file_chmod(FileView *view, char *path, char *mode, int recurse_dirs)
{
  char cmd[PATH_MAX + 128] = " ";
  char *filename = escape_filename(path, strlen(path), 1);

	if (recurse_dirs)
		snprintf(cmd, sizeof(cmd), "chmod -R %s %s", mode, filename);
	else
		snprintf(cmd, sizeof(cmd), "chmod %s %s", mode, filename);

	start_background_job(cmd);

	load_dir_list(view, 1);
	moveto_list_pos(view, view->list_pos);
	free(filename);
}

static void
reset_change_window(void)
{
	curs_set(0);
	werase(change_win);
	update_all_windows();
	if(curr_stats.need_redraw)
		redraw_window();
}

void
change_file_owner(char *file)
{
	// TODO write code
}

void
change_file_group(char *file)
{
	// TODO write code
}

void
set_perm_string(FileView *view, int *perms, char *file)
{
	int i = 0;
	char *add_perm[] = {"u+r", "u+w", "u+x", "u+s", "g+r", "g+w", "g+x", "g+s",
											"o+r", "o+w", "o+x", "o+t"};
	char *sub_perm[] = { "u-r", "u-w", "u-x", "u-s", "g-r", "g-w", "g-x", "g-s",
											"o-r", "o-w", "o-x", "o-t"};
	char perm_string[64] = " ";

	for (i = 0; i < 12; i++)
	{
		if (perms[i])
			strcat(perm_string, add_perm[i]);
		else
			strcat(perm_string, sub_perm[i]);

		strcat(perm_string, ",");
	}
	perm_string[strlen(perm_string) - 1] = '\0'; /* Remove last , */

	file_chmod(view, file, perm_string, perms[12]);
}

static void
permissions_key_cb(FileView *view, int *perms, int isdir)
{
	int done = 0;
	int abort = 0;
	int top = 3;
	int bottom = 16;
	int curr = 3;
	int permnum = 0;
	int step = 1;
	int col = 9;
	char filename[NAME_MAX];
	char path[PATH_MAX];
	int changed = 0;

	if (isdir)
		bottom = 17;

	snprintf(filename, sizeof(filename), "%s",
			view->dir_entry[view->list_pos].name);
	snprintf(path, sizeof(path), "%s/%s", view->curr_dir,
			view->dir_entry[view->list_pos].name);

	curs_set(1);
	wmove(change_win, curr, col);
	wrefresh(change_win);

	while(!done)
	{
		int key = wgetch(change_win);

		switch(key)
		{
			case 'j':
				{
					curr+= step;
					permnum++;

					if(curr > bottom)
					{
						curr-= step;
						permnum--;
					}
					if (curr == 7 || curr == 12)
						curr++;

					wmove(change_win, curr, col);
					wrefresh(change_win);
				}
				break;
			case 'k':
				{
					curr-= step;
					permnum--;
					if(curr < top)
					{
						curr+= step;
						permnum++;
					}

					if (curr == 7 || curr == 12)
						curr--;

					wmove(change_win, curr, col);
					wrefresh(change_win);
				}
				break;
			case 't':
			case 32: /* ascii Spacebar */
				{
					changed++;
					if (perms[permnum])
					{
						perms[permnum] = 0;
						mvwaddch(change_win, curr, col, ' ');
					}
					else
					{
						perms[permnum] = 1;
						mvwaddch(change_win, curr, col, '*');
					}

					wmove(change_win, curr, col);
					wrefresh(change_win);
				}
				break;
			case 3: /* ascii Ctrl C */
			case 27: /* ascii Escape */
				done = 1;
				abort = 1;
				break;
			case 'l':
			case 13: /* ascii Return */
				done = 1;
				break;
			default:
				break;
		}
	}

	reset_change_window();

	curs_set(0);

	if (abort)
	{
		moveto_list_pos(view, find_file_pos_in_list(view, filename));
		return;
	}

	if (changed)
	{
		set_perm_string(view, perms, path);
		load_dir_list(view, 1);
		moveto_list_pos(view, view->curr_line);
	}

}

static void
change_key_cb(FileView *view, int type)
{
	int done = 0;
	int abort = 0;
	int top = 2;
	int bottom = 8;
	int curr = 2;
	int step = 2;
	int col = 6;
	char filename[NAME_MAX];

	snprintf(filename, sizeof(filename), "%s",
			view->dir_entry[view->list_pos].name);

	curs_set(0);
	wmove(change_win, curr, col);
	wrefresh(change_win);

	while(!done)
	{
		int key = wgetch(change_win);

		switch(key)
		{
			case 'j':
				{
					mvwaddch(change_win, curr, col, ' ');
					curr+= step;

					if(curr > bottom)
						curr-= step;

					mvwaddch(change_win, curr, col, '*');
					wmove(change_win, curr, col);
					wrefresh(change_win);
				}
				break;
			case 'k':
				{

					mvwaddch(change_win, curr, col, ' ');
					curr-= step;
					if(curr < top)
						curr+= step;

					mvwaddch(change_win, curr, col, '*');
					wmove(change_win, curr, col);
					wrefresh(change_win);
				}
				break;
			case 3: /* ascii Ctrl C */
			case 27: /* ascii Escape */
				done = 1;
				abort = 1;
				break;
			case 'l':
			case 13: /* ascii Return */
				done = 1;
				break;
			default:
				break;
		}
	}

	reset_change_window();

	if(abort)
	{
		moveto_list_pos(view, find_file_pos_in_list(view, filename));
		return;
	}

	switch(type)
	{
		case FILE_CHANGE:
		{
			if (curr == FILE_NAME)
				rename_file(view);
			else
				show_change_window(view, curr);
			/*
			char * filename = get_current_file_name(view);
			switch(curr)
			{
				case FILE_NAME:
					rename_file(view);
					break;
				case FILE_OWNER:
					change_file_owner(filename);
					break;
				case FILE_GROUP:
					change_file_group(filename);
					break;
				case FILE_PERMISSIONS:
					show_change_window(view, type);
					break;
				default:
					break;
			}
			*/
		}
		break;
		case FILE_NAME:
			break;
		case FILE_OWNER:
			break;
		case FILE_GROUP:
			break;
		case FILE_PERMISSIONS:
			break;
		default:
			break;
	}
}

void
show_file_permissions_menu(FileView *view, int x)
{
	mode_t mode = view->dir_entry[view->list_pos].mode;
	char *filename = get_current_file_name(view);
	int perms[] = {0,0,0,0,0,0,0,0,0,0,0,0,0};
	int isdir = 0;

	if (strlen(filename) > x - 2)
		filename[x - 4] = '\0';

	mvwaddnstr(change_win, 1, (x - strlen(filename))/2, filename, x - 2);

	mvwaddstr(change_win, 3, 2, "Owner [ ] Read");
	if (mode & S_IRUSR)
	{
		perms[0] = 1;
		mvwaddch(change_win, 3, 9, '*');
	}
	mvwaddstr(change_win, 4, 6, "  [ ] Write");

	if (mode & S_IWUSR)
	{
		perms[1] = 1;
		mvwaddch(change_win, 4, 9, '*');
	}
	mvwaddstr(change_win, 5, 6, "  [ ] Execute");

	if (mode & S_IXUSR)
	{
		perms[2] = 1;
		mvwaddch(change_win, 5, 9, '*');
	}

	mvwaddstr(change_win, 6, 6, "  [ ] SetUID");
	if (mode & S_ISUID)
	{
		perms[3] = 1;
		mvwaddch(change_win, 6, 9, '*');
	}

	mvwaddstr(change_win, 8, 2, "Group [ ] Read");
	if (mode & S_IRGRP)
	{
		perms[4] = 1;
		mvwaddch(change_win, 8, 9, '*');
	}

	mvwaddstr(change_win, 9, 6, "  [ ] Write");
	if (mode & S_IWGRP)
	{
		perms[5] = 1;
		mvwaddch(change_win, 9, 9, '*');
	}

	mvwaddstr(change_win, 10, 6, "	[ ] Execute");
	if (mode & S_IXGRP)
	{
		perms[6] = 1;
		mvwaddch(change_win, 10, 9, '*');
	}

	mvwaddstr(change_win, 11, 6, "	[ ] SetGID");
	if (mode & S_ISGID)
	{
		perms[7] = 1;
		mvwaddch(change_win, 11, 9, '*');
	}

	mvwaddstr(change_win, 13, 2, "Other [ ] Read");
	if (mode & S_IROTH)
	{
		perms[8] = 1;
		mvwaddch(change_win, 13, 9, '*');
	}

	mvwaddstr(change_win, 14, 6, "	[ ] Write");
	if (mode & S_IWOTH)
	{
		perms[9] = 1;
		mvwaddch(change_win, 14, 9, '*');
	}

	mvwaddstr(change_win, 15, 6, "	[ ] Execute");
	if (mode & S_IXOTH)
	{
		perms[10] = 1;
		mvwaddch(change_win, 15, 9, '*');
	}

	mvwaddstr(change_win, 16, 6, "	[ ] Sticky");
	if (mode & S_ISVTX)
	{
		perms[11] = 1;
		mvwaddch(change_win, 16, 9, '*');
	}

	if (is_dir(filename))
	{
		mvwaddstr(change_win, 17, 6, "	[ ] Set Recursively");
		isdir = 1;
	}

	permissions_key_cb(view, perms, isdir);
}


void
show_change_window(FileView *view, int type)
{
	int x, y;

	wattroff(view->win, COLOR_PAIR(CURR_LINE_COLOR) | A_BOLD);
	curs_set(0);
	doupdate();
	wclear(change_win);

	getmaxyx(stdscr, y, x);
	mvwin(change_win, (y - 20)/2, (x - 30)/2);
	box(change_win, ACS_VLINE, ACS_HLINE);

	curs_set(1);
	wrefresh(change_win);


	switch(type)
	{
		case FILE_CHANGE:
		{
			mvwaddstr(change_win, 0, (x - 20)/2, " Change Current File ");
			mvwaddstr(change_win, 2, 4, " [ ] Name");
			mvwaddstr(change_win, 4, 4, " [ ] Owner");
			mvwaddstr(change_win, 6, 4, " [ ] Group");
			mvwaddstr(change_win, 8, 4, " [ ] Permissions");
			mvwaddch(change_win, 2, 6, '*');
			change_key_cb(view, type);
		}
			break;
		case FILE_NAME:
			return;
			break;
		case FILE_OWNER:
			return;
			break;
		case FILE_GROUP:
			return;
			break;
		case FILE_PERMISSIONS:
			show_file_permissions_menu(view, x);
			break;
		default:
			break;
	}
}

void
rename_file(FileView *view)
{
	char *filename = get_current_file_name(view);
	char command[1024];
	int key;
	int pos = get_utf8_string_length(filename) + 1;
	int index = strlen(filename);
	int done = 0;
	int abort = 0;
	int real_len = pos - 1;
	int len;
	int found = -1;
	char buf[view->window_width -2];

		len = strlen(filename);
		if (filename[len - 1] == '/')
		{
				filename[len - 1] = '\0';
				len--;
				index--;
				pos--;
		}

	wattroff(view->win, COLOR_PAIR(CURR_LINE_COLOR));
	wmove(view->win, view->curr_line, 0);
	wclrtoeol(view->win);
	wmove(view->win, view->curr_line, 1);
	waddstr(view->win, filename);
	memset(buf, '\0', view->window_width -2);
	strncpy(buf, filename, sizeof(buf));
	wmove(view->win, view->curr_line, pos);

	curs_set(1);

	while(!done)
	{
		if(curr_stats.freeze)
			continue;
		curs_set(1);
		flushinp();
		curr_stats.getting_input = 1;
		key = wgetch(view->win);

		switch(key)
		{
			case 27: /* ascii Escape */
			case 3: /* ascii Ctrl C */
				done = 1;
				abort = 1;
				break;
			case 13: /* ascii Return */
				done = 1;
				break;
				/* This needs to be changed to a value that is read from
				 * the termcap file.
				 */
			case 0x7f: /* This is the one that works on my machine */
			case 8: /* ascii Backspace	ascii Ctrl H */
			case KEY_BACKSPACE: /* ncurses BACKSPACE KEY */
				{
					size_t prev;
					if(index == 0)
						break;

					pos--;
					prev = get_utf8_prev_width(buf, index);

					if(index == len)
					{
						len -= index - prev;
						index = prev;
						if(pos < 1)
							pos = 1;

						mvwdelch(view->win, view->curr_line, pos);
						buf[index] = '\0';
						buf[len] = '\0';
					}
					else
					{
						memmove(buf + prev, buf + index, len - index + 1);
						len -= index - prev;
						index = prev;

						mvwdelch(view->win, view->curr_line,
										 get_utf8_string_length(buf) + 1);
						mvwaddstr(view->win, view->curr_line, pos, buf + index);
						wmove(view->win, view->curr_line, pos);
					}
				}
				break;
			case KEY_DC: /* Delete character */
				{
					size_t cwidth;
					if(index == len)
						break;

					cwidth = get_char_width(buf + index);
					memmove(buf + index, buf + index + cwidth,
									len - index + cwidth + 1);
					len -= cwidth;

					mvwdelch(view->win, view->curr_line,
									 get_utf8_string_length(buf) + 1);
					mvwaddstr(view->win, view->curr_line, pos, buf + index);
					wmove(view->win, view->curr_line, pos);
				}
				break;
			case KEY_LEFT:
				{
					index = get_utf8_prev_width(buf, index);
					pos--;

					if(pos < 1)
						pos = 1;

					wmove(view->win, view->curr_line, pos);

				}
				break;
			case KEY_RIGHT:
				{
					index += get_char_width(buf + index);
					pos++;

					if(pos > len + 1)
						pos = len + 1;

					wmove(view->win, view->curr_line, pos);
				}
				break;
			case KEY_HOME:
				if(index == 0)
					break;

				pos = 1;
				index = 0;
				wmove(view->win, view->curr_line, pos);
				break;
			case KEY_END:
				if(index == len)
					break;

				pos = get_utf8_string_length(buf) + 1;
				index = len;
				wmove(view->win, view->curr_line, pos);
				break;
			case ERR: /* timeout */
				break;
			default:
				{
					size_t i, width;

					if(key < 0x20) /* not a printable character */
						break;

					width = guess_char_width(key);
					memmove(buf + index + width, buf + index, len - index + 1);
					len += width;
					for(i = 0; i < width; ++i)
					{
						buf[index++] = key;
						if(index > 62)
						{
							abort = 1;
							done = 1;
							break;
						}

						len++;

						if (i != width - 1) {
							key = wgetch(view->win);
						}
					}
					if(!done) {
						mvwaddstr(view->win, view->curr_line, pos,
											buf + index - width);
						pos++;
						wmove(view->win, view->curr_line, pos);
					}
				}
				break;
		}
		curr_stats.getting_input = 0;
	}
	curs_set(0);

	if(abort)
	{
		load_dir_list(view, 1);
		moveto_list_pos(view, view->list_pos);
		return;
	}

	if(access(buf, F_OK) == 0 && strncmp(filename, buf, len) != 0)
	{
		show_error_msg("File exists", "That file already exists. Will not overwrite.");

		load_dir_list(view, 1);
		moveto_list_pos(view, view->list_pos);
		return;
	}
	snprintf(command, sizeof(command), "mv -f \'%s\' \'%s\'", filename, buf);

	my_system(command);

	load_dir_list(view, 0);
	found = find_file_pos_in_list(view, buf);
	if(found >= 0)
		moveto_list_pos(view, found);
	else
		moveto_list_pos(view, view->list_pos);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
