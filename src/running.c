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
#ifndef _WIN32
#include <sys/wait.h> /* WEXITSTATUS() */
#endif

#include <limits.h> /* PATH_MAX */
#include <signal.h> /* sighandler_t, signal() */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* malloc() free() */
#include <string.h> /* strcmp() strrchr() strcat() strstr() strlen() strchr()
                       strdup() strncmp() */

#include "cfg/config.h"
#include "cfg/info.h"
#include "menus/menus.h"
#include "utils/env.h"
#include "utils/fs.h"
#include "utils/fs_limits.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/utils.h"
#include "background.h"
#include "file_magic.h"
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

static int is_dir_entry(const char *filename, int type);
static void execute_file(FileView *view, int dont_execute);
static int multi_run_compat(FileView *view, const char *program);
static void follow_link(FileView *view, int follow_dirs);
static void get_last_path_component(const char *path, char* buf);
static int try_run_with_filetype(FileView *view, const assoc_records_t assocs,
		const char start[], int background);

void
handle_file(FileView *view, int dont_execute, int force_follow)
{
	char full[PATH_MAX];
	int executable;
	int runnable;
	const dir_entry_t *curr = &view->dir_entry[view->list_pos];

	snprintf(full, sizeof(full), "%s/%s", view->curr_dir, curr->name);
	chosp(full);

	if(is_dir(full) || is_unc_root(view->curr_dir))
	{
		if(!view->dir_entry[view->list_pos].selected &&
				(curr->type != LINK || !force_follow))
		{
			handle_dir(view);
			return;
		}
	}

	runnable = !cfg.follow_links && curr->type == LINK &&
			!check_link_is_dir(full);
	if(runnable && force_follow)
		runnable = 0;
	if(view->selected_files > 0)
		runnable = 1;
	runnable = curr->type == REGULAR || curr->type == EXECUTABLE || runnable ||
			curr->type == DIRECTORY;

#ifndef _WIN32
	executable = curr->type == EXECUTABLE ||
			(runnable && access(full, X_OK) == 0 &&
			S_ISEXE(view->dir_entry[view->list_pos].mode));
#else
	executable = curr->type == EXECUTABLE;
#endif
	executable = executable && !dont_execute && cfg.auto_execute;

	if(cfg.vim_filter && (executable || runnable))
		use_vim_plugin(view, 0, NULL); /* no return */

	if(executable && !is_dir_entry(full, curr->type))
	{
#ifndef _WIN32
		shellout(full, 1, 1);
#else
		to_back_slash(full);
		if(curr_stats.as_admin && is_vista_and_above())
		{
			SHELLEXECUTEINFOA sei;
			memset(&sei, 0, sizeof(sei));
			sei.cbSize = sizeof(sei);
			sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
			sei.lpVerb = "runas";
			sei.lpFile = full;
			sei.lpParameters = NULL;
			sei.nShow = SW_SHOWNORMAL;

			if(ShellExecuteEx(&sei))
				CloseHandle(sei.hProcess);
		}
		else
		{
			exec_program(full);
			update_screen(UT_FULL);
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
				curr = &view->dir_entry[i];
				if(!curr->selected)
					continue;

				snprintf(full, sizeof(full), "%s/%s", view->curr_dir, curr->name);
				chosp(full);
				if(is_dir_entry(full, curr->type))
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

		execute_file(view, dont_execute);
	}
	else if(curr->type == LINK)
	{
		follow_link(view, force_follow);
	}
}

static int
is_dir_entry(const char *filename, int type)
{
	return type == DIRECTORY || (type == LINK && is_dir(filename));
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

		if(!path_exists(view->dir_entry[i].name))
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
		show_error_msg("Selection error", "Files have different programs");
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
				start_background_job(cmd, 0);
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

	if(!path_exists(filename))
	{
		show_error_msg("Broken Link", "Link destination doesn't exist");
		return;
	}

#ifndef _WIN32
	escaped = escape_filename(filename, 0);
#else
	escaped = (char *)enclose_in_dquotes(filename);
#endif

	snprintf(vicmd, sizeof(vicmd), "%s", get_vicmd(&bg));
	(void)trim_right(vicmd);
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
		start_background_job(command, 0);
	else
		shellout(command, -1, 0);
	curs_set(FALSE);
}

char *
edit_selection(FileView *view, int *bg)
{
	char *buf;
	char *files = expand_macros(view, "%f", NULL, NULL);

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
	int pause = starts_with(program, "!!");
	if(pause)
		program += 2;

	if(!path_exists_at(view->curr_dir, view->dir_entry[view->list_pos].name))
	{
		show_error_msg("Access Error", "File doesn't exist.");
		return;
	}

	if(has_mount_prefixes(program))
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
	else if(strcmp(program, VIFM_PSEUDO_CMD) == 0)
	{
		handle_dir(view);
	}
	else if(strchr(program, '%') != NULL)
	{
		int background;
		MacroFlags flags;
		char *command = expand_macros(view, program, NULL, &flags);

		background = ends_with(command, " &");
		if(background)
			command[strlen(command) - 2] = '\0';

		if(!pause && (background || force_background))
			start_background_job(command, flags == MACRO_IGNORE);
		else if(flags == MACRO_IGNORE)
			output_to_nowhere(command);
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
		show_error_msg("Error", "Can't read link");
		return;
	}

	if(!path_exists(linkto))
	{
		show_error_msg("Broken Link",
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
			file = after_last(link_dup, '/');
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
	char full_path[PATH_MAX];
	char *filename;

	filename = get_current_file_name(view);

	if(stroscmp(filename, "../") == 0)
	{
		cd_updir(view);
		return;
	}

	snprintf(full_path, sizeof(full_path), "%s%s%s", view->curr_dir,
			ends_with_slash(view->curr_dir) ? "" : "/", filename);
	if(!cd_is_possible(full_path))
	{
		return;
	}

	if(change_directory(view, filename) >= 0)
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

	char buf[cfg.max_args];
	int result;
	int ec;

	if(pause > 0 && command != NULL && ends_with(command, "&"))
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
				if(stroscmp(cfg.shell, "cmd") == 0)
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
	/* No WIFEXITED(ec) check here, since my_system(...) shouldn't return until
	 * subprocess exited. */
	result = WEXITSTATUS(ec);

#ifndef _WIN32
	if(result != 0 && pause < 0)
		my_system(PAUSE_CMD);
#endif

	/* force views update */
	request_view_update(&lwin);
	request_view_update(&rwin);

#ifdef _WIN32
	reset_prog_mode();
	resize_term(cfg.lines, cfg.columns);
#endif
	/* always redraw to handle resizing of terminal */
	if(!curr_stats.auto_redraws)
		curr_stats.need_update = UT_FULL;

	curs_set(FALSE);

	return result;
}

void
output_to_nowhere(const char *cmd)
{
	FILE *file, *err;

	if(background_and_capture((char *)cmd, &file, &err) != 0)
	{
		show_error_msgf("Trouble running command", "Unable to run: %s", cmd);
		return;
	}

	fclose(file);
	fclose(err);
}

int
run_with_filetype(FileView *view, const char beginning[], int background)
{
	char *filename = get_current_file_name(view);
	assoc_records_t ft = get_all_programs_for_file(filename);
	assoc_records_t magic = get_magic_handlers(filename);

	if(try_run_with_filetype(view, ft, beginning, background))
	{
		free(ft.list);
		return 0;
	}

	free(ft.list);

	return !try_run_with_filetype(view, magic, beginning, background);
}

/* Returns non-zero on successful running. */
static int
try_run_with_filetype(FileView *view, const assoc_records_t assocs,
		const char start[], int background)
{
	const size_t len = strlen(start);
	int i;
	for(i = 0; i < assocs.count; i++)
	{
		if(strncmp(assocs.list[i].command, start, len) == 0)
		{
			run_using_prog(view, assocs.list[i].command, 0, background);
			return 1;
		}
	}
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
