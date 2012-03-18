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

#include <sys/stat.h> /* stat */

#include <stdio.h> /* snprintf() */
#include <string.h> /* strcmp() strrchr() strcat() strstr() strlen() strchr()
                       strdup() */

#include "cfg/config.h"
#include "cfg/info.h"
#include "menus/menus.h"
#include "utils/macros.h"
#include "utils/utils.h"
#include "background.h"
#include "filelist.h"
#include "filetype.h"
#include "fuse.h"
#include "macros.h"
#include "status.h"
#include "ui.h"

#include "running.h"

#ifndef _WIN32
#define PAUSE_CMD "vifm-pause"
#define PAUSE_STR "; "PAUSE_CMD
#else
#define PAUSE_CMD "vifm-pause"
#define PAUSE_STR " && pause || pause"
#endif

static void execute_file(FileView *view, int dont_execute);
static int multi_run_compat(FileView *view, const char *program);
static void follow_link(FileView *view, int follow_dirs);
static void get_last_path_component(const char *path, char* buf);

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

	if(is_dir(name) || is_unc_root(view->curr_dir))
	{
		if(!view->dir_entry[view->list_pos].selected &&
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

#ifndef _WIN32
	executable = type == EXECUTABLE || (runnable && access(filename, X_OK) == 0 &&
			S_ISEXE(view->dir_entry[view->list_pos].mode));
#else
	executable = type == EXECUTABLE;
#endif
	executable = executable && !dont_execute && cfg.auto_execute;

	if(cfg.vim_filter && (executable || runnable))
		use_vim_plugin(view, 0, NULL); /* no return */

	if(executable && type != DIRECTORY)
	{
		char buf[NAME_MAX];
		snprintf(buf, sizeof(buf), "%s/%s", view->curr_dir, filename);
#ifndef _WIN32
		shellout(buf, 1, 1);
#else
		to_back_slash(buf);
		if(curr_stats.as_admin && is_vista_and_above())
		{
			SHELLEXECUTEINFOA sei;
			memset(&sei, 0, sizeof(sei));
			sei.cbSize = sizeof(sei);
			sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
			sei.lpVerb = "runas";
			sei.lpFile = buf;
			sei.lpParameters = NULL;
			sei.nShow = SW_SHOWNORMAL;

			if(ShellExecuteEx(&sei))
				CloseHandle(sei.hProcess);
		}
		else
		{
			exec_program(buf);
			redraw_window();
		}
#endif
	}
	else if(runnable)
	{
		if(view->selected_files > 1)
		{
			int files = 0, dirs = 0;
			int i;
			for(i = 0; i < view->list_rows; i++)
			{
				int type = view->dir_entry[i].type;
				if(!view->dir_entry[i].selected)
					continue;
				if(type == DIRECTORY ||
						(type == LINK && is_dir(view->dir_entry[i].name)))
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

		execute_file(view, dont_execute);
	}
	else if(type == LINK)
	{
		follow_link(view, force_follow);
	}
}

static void
execute_file(FileView *view, int dont_execute)
{
	/* TODO: refactor this function execute_file() */

	assoc_record_t program = {};
	int undef;
	int same;
	int i;
	int no_multi_run = 0;

	if(!view->dir_entry[view->list_pos].selected)
		clean_selected_files(view);

	(void)get_default_program_for_file(view->dir_entry[view->list_pos].name,
			&program);
	no_multi_run += !multi_run_compat(view, program.command);
	undef = 0;
	same = 1;
	for(i = 0; i < view->list_rows; i++)
	{
		assoc_record_t prog;

		if(!view->dir_entry[i].selected)
			continue;

		if(access(view->dir_entry[i].name, F_OK) != 0)
		{
			show_error_msgf("Broken Link", "Destination of \"%s\" link doesn't exist",
					view->dir_entry[i].name);
			free_assoc_record(&program);
			return;
		}

		if(get_default_program_for_file(view->dir_entry[i].name, &prog))
		{
			no_multi_run += !multi_run_compat(view, prog.command);
			if(assoc_prog_is_empty(&program))
			{
				free_assoc_record(&program);
				program = prog;
			}
			else
			{
				if(strcmp(prog.command, program.command) != 0)
					same = 0;
				free_assoc_record(&prog);
			}
		}
		else
			undef++;
	}

	if(!same && undef == 0 && no_multi_run)
	{
		free_assoc_record(&program);
		(void)show_error_msg("Selection error", "Files have different programs");
		return;
	}
	if(undef > 0)
	{
		free_assoc_record(&program);
	}

	/* Check for a filetype */
	/* vi is set as the default for any extension without a program */
	if(program.command == NULL)
	{
		if(view->dir_entry[view->list_pos].type == DIRECTORY)
		{
			handle_dir(view);
		}
		else if(view->selected_files <= 1)
		{
			char buf[PATH_MAX];
			snprintf(buf, sizeof(buf), "%s/%s", view->curr_dir,
					get_current_file_name(view));
			view_file(buf, -1, 1);
		}
		else
		{
			int bg;
			char *cmd = edit_selection(view, &bg);
			if(bg)
				start_background_job(cmd);
			else
				shellout(cmd, -1, 1);
			free(cmd);
		}
		return;
	}

	if(!no_multi_run)
	{
		int pos = view->list_pos;
		free_assoc_record(&program);

		for(i = 0; i < view->list_rows; i++)
		{
			if(!view->dir_entry[i].selected)
				continue;
			view->list_pos = i;
			(void)get_default_program_for_file(view->dir_entry[view->list_pos].name,
					&program);
			run_using_prog(view, program.command, dont_execute, 0);
			free_assoc_record(&program);
		}
		view->list_pos = pos;
	}
	else
	{
		run_using_prog(view, program.command, dont_execute, 0);
		free_assoc_record(&program);
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

/* negative line means ignore it */
void
view_file(const char *filename, int line, int do_fork)
{
	char vicmd[PATH_MAX];
	char command[PATH_MAX + 5] = "";
	const char *fork_str = do_fork ? "" : "--nofork";
	char *escaped;
	int bg;

	if(access(filename, F_OK) != 0)
	{
		(void)show_error_msg("Broken Link", "Link destination doesn't exist");
		return;
	}

#ifndef _WIN32
	escaped = escape_filename(filename, 0);
#else
	escaped = (char *)enclose_in_dquotes(filename);
#endif

	snprintf(vicmd, sizeof(vicmd), "%s", get_vicmd(&bg));
	if(!do_fork)
	{
		char *p = strrchr(vicmd, ' ');
		if(p != NULL && strstr(p, "remote"))
		{
			*p = '\0';
		}
	}

	if(line < 0)
		snprintf(command, sizeof(command), "%s %s %s", vicmd, fork_str, escaped);
	else
		snprintf(command, sizeof(command), "%s %s +%d %s", vicmd, fork_str, line,
				escaped);
#ifndef _WIN32
	free(escaped);
#endif

	if(bg && do_fork)
		start_background_job(command);
	else
		shellout(command, -1, 0);
	curs_set(FALSE);
}

char *
edit_selection(FileView *view, int *bg)
{
	int use_menu = 0;
	int split = 0;
	char *buf;
	char *files = expand_macros(view, "%f", NULL, &use_menu, &split);

	if((buf = malloc(strlen(get_vicmd(bg)) + strlen(files) + 2)) != NULL)
		snprintf(buf, strlen(get_vicmd(bg)) + 1 + strlen(files) + 1, "%s %s",
				get_vicmd(bg), files);

	free(files);
	return buf;
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
		{
			char buf[PATH_MAX];
			snprintf(buf, sizeof(buf), "%s/%s", view->curr_dir,
					get_current_file_name(view));
			view_file(buf, -1, 1);
		}
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
			shellout(command, pause ? 1 : -1, 1);

		free(command);
	}
	else
	{
		char buf[NAME_MAX + 1 + NAME_MAX + 1];
		char *temp = escape_filename(view->dir_entry[view->list_pos].name, 0);

		snprintf(buf, sizeof(buf), "%s %s", program, temp);
		shellout(buf, pause ? 1 : -1, 1);
		free(temp);
	}
}

static void
follow_link(FileView *view, int follow_dirs)
{
	/* TODO: refactor this big function shellout() */

	struct stat s;
	int is_dir = 0;
	char *dir = NULL, *file = NULL, *link_dup;
	char buf[PATH_MAX];
	char linkto[PATH_MAX + NAME_MAX];
	char *filename;

	filename = view->dir_entry[view->list_pos].name;
	snprintf(buf, sizeof(buf), "%s/%s", view->curr_dir, filename);

	if(get_link_target(buf, linkto, sizeof(linkto)) != 0)
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
				if(lstat(linkto, &s) != 0)
				{
					strcat(linkto, "/");
					lstat(linkto, &s);
				}
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
handle_dir(FileView *view)
{
	char *filename;

	filename = get_current_file_name(view);

	if(pathcmp(filename, "../") == 0)
	{
		cd_updir(view);
	}
	else if(change_directory(view, filename) >= 0)
	{
		load_dir_list(view, 0);
		move_to_list_pos(view, view->list_pos);
	}
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
			fprintf(fp, "%s", view->curr_dir);
			if(view->curr_dir[strlen(view->curr_dir) - 1] != '/')
				fprintf(fp, "%s", "/");
			fprintf(fp, "%s\n", view->dir_entry[view->list_pos].name);
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

/*
 * pause:
 *  > 0 - pause always
 *  = 0 - do not pause
 *  < 0 - pause on error
 */
int
shellout(const char *command, int pause, int allow_screen)
{
	/* TODO: refactor this big function shellout() */

	size_t len = (command != NULL) ? strlen(command) : 0;
	char buf[cfg.max_args];
	int result;
	int ec;

	if(pause > 0 && len > 1 && command[len - 1] == '&')
		pause = -1;

	if(command != NULL)
	{
		if(allow_screen && cfg.use_screen)
		{
			int bg;
			char *escaped;
			char *ptr = NULL;
			char *title = strstr(command, get_vicmd(&bg));
			char *escaped_sh = escape_filename(cfg.shell, 0);

			/* Needed for symlink directories and sshfs mounts */
			escaped = escape_filename(curr_view->curr_dir, 0);
			snprintf(buf, sizeof(buf), "screen -X setenv PWD %s", escaped);
			free(escaped);

			my_system(buf);

			if(title != NULL)
			{
				if(pause > 0)
				{
					snprintf(buf, sizeof(buf),
							"screen -t \"%s\" %s -c '%s" PAUSE_STR "'",
							title + strlen(get_vicmd(&bg)) + 1, escaped_sh, command);
				}
				else
				{
					escaped = escape_filename(command, 0);
					snprintf(buf, sizeof(buf), "screen -t \"%s\" %s -c %s",
							title + strlen(get_vicmd(&bg)) + 1, escaped_sh, escaped);
					free(escaped);
				}
			}
			else
			{
				ptr = strchr(command, ' ');
				if(ptr != NULL)
				{
					*ptr = '\0';
					title = strdup(command);
					*ptr = ' ';
				}
				else
				{
					title = strdup("Shell");
				}

				if(pause > 0)
				{
					snprintf(buf, sizeof(buf),
							"screen -t \"%.10s\" %s -c '%s" PAUSE_STR "'", title,
							escaped_sh, command);
				}
				else
				{
					escaped = escape_filename(command, 0);
					snprintf(buf, sizeof(buf), "screen -t \"%.10s\" %s -c %s", title,
							escaped_sh, escaped);
					free(escaped);
				}
				free(title);
			}
			free(escaped_sh);
		}
		else
		{
			if(pause > 0)
			{
#ifdef _WIN32
				if(pathcmp(cfg.shell, "cmd") == 0)
					snprintf(buf, sizeof(buf), "%s" PAUSE_STR, command);
				else
#endif
					snprintf(buf, sizeof(buf), "%s; " PAUSE_CMD, command);
			}
			else
			{
				snprintf(buf, sizeof(buf), "%s", command);
			}
		}
	}
	else
	{
		if(allow_screen && cfg.use_screen)
		{
			snprintf(buf, sizeof(buf), "screen -X setenv PWD \'%s\'",
					curr_view->curr_dir);

			my_system(buf);

			snprintf(buf, sizeof(buf), "screen");
		}
		else
		{
			snprintf(buf, sizeof(buf), "%s", cfg.shell);
		}
	}

	def_prog_mode();
	endwin();

	/* Need to use setenv instead of getcwd for a symlink directory */
	env_set("PWD", curr_view->curr_dir);

	ec = my_system(buf);
	result = WEXITSTATUS(ec);

#ifndef _WIN32
	if(result != 0 && pause < 0)
		my_system(PAUSE_CMD);

	/* force views update */
	memset(&lwin.dir_mtime, 0, sizeof(lwin.dir_mtime));
	memset(&rwin.dir_mtime, 0, sizeof(rwin.dir_mtime));
#endif

#ifdef _WIN32
	reset_prog_mode();
	resize_term(cfg.lines, cfg.columns);
#endif
	/* always redraw to handle resizing of terminal */
	if(!curr_stats.auto_redraws)
		curr_stats.need_redraw = 1;

	curs_set(FALSE);

	return result;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
