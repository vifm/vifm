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

#include "running.h"

#include <curses.h> /* FALSE curs_set() endwin() */

#include <sys/stat.h> /* stat */
#ifndef _WIN32
#include <sys/wait.h> /* WEXITSTATUS() */
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#ifndef ERROR_ELEVATION_REQUIRED /* Windows Vista and later. */
#define ERROR_ELEVATION_REQUIRED 740L
#endif
#endif
#include <unistd.h> /* pid_t */

#include <assert.h> /* assert() */
#include <errno.h> /* errno */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* EXIT_FAILURE EXIT_SUCCESS malloc() free() */
#include <string.h> /* strcmp() strerror() strrchr() strcat() strstr() strlen()
                       strchr() strdup() strncmp() */

#include "cfg/config.h"
#include "cfg/info.h"
#include "compat/os.h"
#include "modes/dialogs/msg_dialog.h"
#include "ui/ui.h"
#include "utils/env.h"
#include "utils/fs.h"
#include "utils/fs_limits.h"
#include "utils/log.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/test_helpers.h"
#include "utils/utils.h"
#include "utils/utf8.h"
#include "background.h"
#include "file_magic.h"
#include "filelist.h"
#include "filetype.h"
#include "fuse.h"
#include "macros.h"
#include "status.h"
#include "types.h"
#include "vim.h"

/* Kinds of symbolic link file treatment on file handling. */
typedef enum
{
	FHL_NO_FOLLOW, /* Don't follow (navigate to instead of navigation inside). */
	FHL_FOLLOW,    /* Follow (end up on the link target, not inside it). */
}
FileHandleLink;

static void handle_file(FileView *view, FileHandleExec exec,
		FileHandleLink follow);
static int is_runnable(const FileView *const view, const char full_path[],
		int type, int force_follow);
static int is_executable(const char full_path[], const dir_entry_t *curr,
		int dont_execute, int runnable);
static int is_dir_entry(const char full_path[], int type);
#ifdef _WIN32
static void run_win_executable(char full_path[], int elevate);
static int run_win_executable_as_evaluated(const char full_path[]);
#endif
static int selection_is_consistent(const FileView *const view);
static void execute_file(const char full_path[], int elevate);
static void run_selection(FileView *view, int dont_execute);
static void run_file(FileView *view, int dont_execute);
static void run_with_defaults(FileView *view);
static void run_selection_separately(FileView *view, int dont_execute);
static int is_multi_run_compat(FileView *view, const char prog_cmd[]);
static void view_current_file(const FileView *view);
static void follow_link(FileView *view, int follow_dirs);
static void extract_last_path_component(const char path[], char buf[]);
static char * gen_shell_cmd(const char cmd[], int pause,
		int use_term_multiplexer);
static char * gen_term_multiplexer_cmd(const char cmd[], int pause);
static char * gen_term_multiplexer_title_arg(const char cmd[]);
static char * gen_normal_cmd(const char cmd[], int pause);
static char * gen_term_multiplexer_run_cmd(void);
static void set_pwd_in_screen(const char path[]);
static int try_run_with_filetype(FileView *view, const assoc_records_t assocs,
		const char start[], int background);

void
open_file(FileView *view, FileHandleExec exec)
{
	handle_file(view, exec, FHL_NO_FOLLOW);
}

void
follow_file(FileView *view)
{
	if(flist_custom_active(view))
	{
		navigate_to(view, view->dir_entry[view->list_pos].origin);
	}
	else
	{
		handle_file(view, FHE_RUN, FHL_FOLLOW);
	}
}

static void
handle_file(FileView *view, FileHandleExec exec, FileHandleLink follow)
{
	char full_path[PATH_MAX];
	int executable;
	int runnable;
	const dir_entry_t *const curr = &view->dir_entry[view->list_pos];

	get_full_path_of(curr, sizeof(full_path), full_path);

	if(is_dir(full_path) || is_unc_root(view->curr_dir))
	{
		if(!curr->selected && (curr->type != FT_LINK || follow == FHL_NO_FOLLOW))
		{
			open_dir(view);
			return;
		}
	}

	runnable = is_runnable(view, full_path, curr->type, follow == FHL_FOLLOW);
	executable = is_executable(full_path, curr, exec == FHE_NO_RUN, runnable);

	if(curr_stats.file_picker_mode && (executable || runnable))
	{
		/* The call below does not return. */
		vim_return_file_list(view, 0, NULL);
	}

	if(executable && !is_dir_entry(full_path, curr->type))
	{
		execute_file(full_path, exec == FHE_ELEVATE_AND_RUN);
	}
	else if(runnable)
	{
		run_selection(view, exec == FHE_NO_RUN);
	}
	else if(curr->type == FT_LINK)
	{
		follow_link(view, follow == FHL_FOLLOW);
	}
}

/* Returns non-zero if file can be executed or it's link to a directory (it can
 * be entered), otherwise zero is returned. */
static int
is_runnable(const FileView *const view, const char full_path[], int type,
		int force_follow)
{
	int runnable = !cfg.follow_links && type == FT_LINK &&
		get_symlink_type(full_path) != SLT_DIR;
	if(runnable && force_follow)
	{
		runnable = 0;
	}
	if(view->selected_files > 0)
	{
		runnable = 1;
	}
	if(!runnable)
	{
		runnable = type == FT_REG || type == FT_EXEC || type == FT_DIR;
	}
	return runnable;
}

/* Returns non-zero if file can be executed, otherwise zero is returned. */
static int
is_executable(const char full_path[], const dir_entry_t *curr, int dont_execute,
		int runnable)
{
	int executable;
#ifndef _WIN32
	executable = curr->type == FT_EXEC ||
			(runnable && os_access(full_path, X_OK) == 0 && S_ISEXE(curr->mode));
#else
	executable = curr->type == FT_EXEC;
#endif
	return executable && !dont_execute && cfg.auto_execute;
}

/* Returns non-zero if entry is directory or link to a directory, otherwise zero
 * is returned. */
static int
is_dir_entry(const char full_path[], int type)
{
	return type == FT_DIR || (type == FT_LINK && is_dir(full_path));
}

#ifdef _WIN32

/* Runs a Windows executable handling errors and rights elevation. */
static void
run_win_executable(char full_path[], int elevate)
{
	int running_error = 0;
	int running_error_code = NO_ERROR;
	if(elevate && is_vista_and_above())
	{
		running_error = run_win_executable_as_evaluated(full_path);
	}
	else
	{
		int returned_exit_code;
		const int error = win_exec_cmd(full_path, &returned_exit_code);
		if(error != 0 && !returned_exit_code)
		{
			if(error == ERROR_ELEVATION_REQUIRED && is_vista_and_above())
			{
				const int user_response = query_user_menu("Program running error",
						"Executable requires rights elevation. Run with elevated rights?");
				if(user_response != 0)
				{
					running_error = run_win_executable_as_evaluated(full_path);
				}
			}
			else
			{
				running_error = 1;
				running_error_code = error;
			}
		}
		update_screen(UT_FULL);
	}
	if(running_error)
	{
		char err_msg[512];
		err_msg[0] = '\0';
		if(running_error_code != NO_ERROR && FormatMessageA(
				FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM, NULL,
				running_error_code, 0, err_msg, sizeof(err_msg), NULL) == 0)
		{
			LOG_WERROR(GetLastError());
		}

		show_error_msgf("Program running error", "Can't run an executable%s%s",
				(err_msg[0] == '\0') ? "." : ": ", err_msg);
	}
}

/* Returns non-zero on error, otherwise zero is returned. */
static int
run_win_executable_as_evaluated(const char full_path[])
{
	wchar_t *utf16_path;
	SHELLEXECUTEINFOW sei;

	utf16_path = utf8_to_utf16(full_path);

	memset(&sei, 0, sizeof(sei));
	sei.cbSize = sizeof(sei);
	sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
	sei.lpVerb = L"runas";
	sei.lpFile = utf16_path;
	sei.lpParameters = NULL;
	sei.nShow = SW_SHOWNORMAL;

	if(!ShellExecuteExW(&sei))
	{
		const DWORD last_error = GetLastError();
		free(utf16_path);
		LOG_WERROR(last_error);
		return last_error != ERROR_CANCELLED;
	}

	free(utf16_path);
	CloseHandle(sei.hProcess);
	return 0;
}

#endif /* _WIN32 */

/* Returns non-zero if selection doesn't mix files and directories, otherwise
 * zero is returned. */
static int
selection_is_consistent(const FileView *const view)
{
	if(view->selected_files > 1)
	{
		int files = 0, dirs = 0;
		int i;
		for(i = 0; i < view->list_rows; i++)
		{
			char full[PATH_MAX];
			const dir_entry_t *const curr = &view->dir_entry[i];
			if(!curr->selected)
			{
				continue;
			}

			get_full_path_of(curr, sizeof(full), full);
			if(is_dir_entry(full, curr->type))
			{
				dirs++;
			}
			else
			{
				files++;
			}
		}
		if(dirs > 0 && files > 0)
		{
			return 0;
		}
	}
	return 1;
}

/* Executes file, specified by the full_path.  Changes type of slashes on
 * Windows. */
static void
execute_file(const char full_path[], int elevate)
{
#ifndef _WIN32
	char *const escaped = escape_filename(full_path, 0);
	shellout(escaped, 1, 1);
	free(escaped);
#else
	char *const dquoted_full_path = strdup(enclose_in_dquotes(full_path));

	to_back_slash(dquoted_full_path);
	run_win_executable(dquoted_full_path, elevate);

	free(dquoted_full_path);
#endif
}

/* Tries to run selection displaying error message on file type
 * inconsistency. */
static void
run_selection(FileView *view, int dont_execute)
{
	if(selection_is_consistent(view))
	{
		run_file(view, dont_execute);
	}
	else
	{
		show_error_msg("Selection error",
				"Selection cannot contain files and directories at the same time");
	}
}

static void
run_file(FileView *view, int dont_execute)
{
	/* TODO: refactor this function run_file() */

	char *typed_fname;
	const char *multi_prog_cmd;
	int undef;
	int same;
	dir_entry_t *entry;
	int no_multi_run;

	if(!view->dir_entry[view->list_pos].selected)
	{
		clean_selected_files(view);
	}

	typed_fname = get_typed_current_fname(view);
	multi_prog_cmd = ft_get_program(typed_fname);
	free(typed_fname);

	no_multi_run = !is_multi_run_compat(view, multi_prog_cmd);
	undef = 0;
	same = 1;

	entry = NULL;
	while(iter_selected_entries(view, &entry))
	{
		char *typed_fname;
		const char *entry_prog_cmd;

		if(!path_exists(entry->name, DEREF))
		{
			show_error_msgf("Broken Link", "Destination of \"%s\" link doesn't exist",
					entry->name);
			return;
		}

		typed_fname = get_typed_entry_fname(entry);
		entry_prog_cmd = ft_get_program(typed_fname);
		free(typed_fname);

		if(entry_prog_cmd == NULL)
		{
			++undef;
			continue;
		}

		no_multi_run += !is_multi_run_compat(view, entry_prog_cmd);
		if(multi_prog_cmd == NULL)
		{
			multi_prog_cmd = entry_prog_cmd;
		}
		else if(strcmp(entry_prog_cmd, multi_prog_cmd) != 0)
		{
			same = 0;
		}
	}

	if(!same && undef == 0 && no_multi_run)
	{
		show_error_msg("Run error", "Handlers of selected files are incompatible.");
		return;
	}
	if(undef > 0)
	{
		multi_prog_cmd = NULL;
	}

	/* Check for a filetype */
	/* vi is set as the default for any extension without a program */
	if(multi_prog_cmd == NULL)
	{
		run_with_defaults(view);
		return;
	}

	if(no_multi_run)
	{
		run_using_prog(view, multi_prog_cmd, dont_execute, 0);
	}
	else
	{
		run_selection_separately(view, dont_execute);
	}
}

/* Runs current file entry of the view in a generic way (entering directories
 * and opening files in editors). */
static void
run_with_defaults(FileView *view)
{
	if(view->dir_entry[view->list_pos].type == FT_DIR)
	{
		open_dir(view);
	}
	else if(view->selected_files <= 1)
	{
		view_current_file(view);
	}
	else if(vim_edit_selection() != 0)
	{
		show_error_msg("Running error", "Can't edit selection");
	}
}

/* Runs each of selected file entries of the view individually. */
static void
run_selection_separately(FileView *view, int dont_execute)
{
	dir_entry_t *entry;

	const int pos = view->list_pos;

	entry = NULL;
	while(iter_selected_entries(view, &entry))
	{
		char *typed_fname;
		const char *entry_prog_cmd;

		typed_fname = get_typed_entry_fname(entry);
		entry_prog_cmd = ft_get_program(typed_fname);
		free(typed_fname);

		view->list_pos = entry_to_pos(view, entry);
		run_using_prog(view, entry_prog_cmd, dont_execute, 0);
	}

	view->list_pos = pos;
}

/* Checks whether command is compatible with firing multiple file handlers for a
 * set of selected files.  Returns non-zero if so, otherwise zero is
 * returned. */
static int
is_multi_run_compat(FileView *view, const char prog_cmd[])
{
	size_t len;
	if(prog_cmd == NULL)
		return 0;
	if(view->selected_files <= 1)
		return 0;
	if((len = strlen(prog_cmd)) == 0)
		return 0;
	if(prog_cmd[len - 1] != '&')
		return 0;
	if(strstr(prog_cmd, "%f") != NULL || strstr(prog_cmd, "%F") != NULL)
		return 0;
	if(strstr(prog_cmd, "%c") == NULL && strstr(prog_cmd, "%C") == NULL)
		return 0;
	return 1;
}

void
run_using_prog(FileView *view, const char program[], int dont_execute,
		int force_background)
{
	const dir_entry_t *const entry = &view->dir_entry[view->list_pos];
	const int pause = starts_with(program, "!!");

	if(pause)
	{
		program += 2;
	}

	if(!path_exists_at(entry->origin, entry->name, DEREF))
	{
		show_error_msg("Access Error", "File doesn't exist.");
		return;
	}

	if(fuse_is_mount_string(program))
	{
		if(dont_execute)
		{
			view_current_file(view);
		}
		else
		{
			fuse_try_mount(view, program);
		}
	}
	else if(strcmp(program, VIFM_PSEUDO_CMD) == 0)
	{
		open_dir(view);
	}
	else if(strchr(program, '%') != NULL)
	{
		int background;
		MacroFlags flags;
		char *command = expand_macros(program, NULL, &flags, 1);

		background = ends_with(command, " &");
		if(background)
			command[strlen(command) - 2] = '\0';

		if(!pause && (background || force_background))
			start_background_job(command, flags == MACRO_IGNORE);
		else if(flags == MACRO_IGNORE)
			output_to_nowhere(command);
		else
			shellout(command, pause ? 1 : -1, flags != MACRO_NO_TERM_MUX);

		free(command);
	}
	else
	{
		char buf[NAME_MAX + 1 + NAME_MAX + 1];
		const char *name_macro;
		char *file_name;

#ifdef _WIN32
		if(curr_stats.shell_type == ST_CMD)
		{
			name_macro = (view == curr_view) ? "%\"c" : "%\"C";
		}
		else
#endif
		{
			name_macro = (view == curr_view) ? "%c" : "%C";
		}

		file_name = expand_macros(name_macro, NULL, NULL, 1);

		snprintf(buf, sizeof(buf), "%s %s", program, file_name);
		shellout(buf, pause ? 1 : -1, 1);

		free(file_name);
	}
}

/* Opens file under the cursor in the viewer. */
static void
view_current_file(const FileView *view)
{
	char full_path[PATH_MAX];
	get_current_full_path(view, sizeof(full_path), full_path);
	(void)vim_view_file(full_path, -1, -1, 1);
}

/* Resolve link target and either navigate inside directory link points to or
 * navigate to directory where target is located pointing cursor on
 * it (the follow_dirs flag controls behaviour). */
static void
follow_link(FileView *view, int follow_dirs)
{
	char *dir, *file;
	char full_path[PATH_MAX];
	char linkto[PATH_MAX + NAME_MAX];
	dir_entry_t *const entry = &curr_view->dir_entry[curr_view->list_pos];

	get_full_path_of(entry, sizeof(full_path), full_path);

	if(get_link_target_abs(full_path, entry->origin, linkto, sizeof(linkto)) != 0)
	{
		show_error_msg("Error", "Can't read link.");
		return;
	}

	if(!path_exists(linkto, DEREF))
	{
		show_error_msg("Broken Link",
				"Can't access link destination.  It might be broken.");
		return;
	}

	chosp(linkto);

	if(is_dir(linkto) && !follow_dirs)
	{
		dir = strdup(entry->name);
		file = NULL;
	}
	else
	{
		dir = strdup(linkto);
		remove_last_path_component(dir);

		file = get_last_path_component(linkto);
	}

	if(dir[0] != '\0')
	{
		navigate_to(view, dir);
	}

	if(file != NULL)
	{
		const int pos = find_file_pos_in_list(view, file);
		if(pos >= 0)
		{
			move_to_list_pos(view, pos);
		}
	}

	free(dir);
}

void
open_dir(FileView *view)
{
	char full_path[PATH_MAX];
	const char *filename;

	filename = get_current_file_name(view);

	if(is_parent_dir(filename))
	{
		cd_updir(view);
		return;
	}

	get_current_full_path(view, sizeof(full_path), full_path);

	if(cd_is_possible(full_path))
	{
		navigate_to(view, full_path);
	}
}

void
cd_updir(FileView *view)
{
	char dir_name[strlen(view->curr_dir) + 1];

	if(flist_custom_active(view))
	{
		navigate_to(view, view->custom.orig_dir);
		return;
	}

	dir_name[0] = '\0';

	extract_last_path_component(view->curr_dir, dir_name);

	if(change_directory(view, "../") != 1)
	{
		load_dir_list(view, 0);
		move_to_list_pos(view, find_file_pos_in_list(view, dir_name));
	}
}

/* Extracts last part of the path into buf.  Assumes that size of the buf is
 * large enough. */
static void
extract_last_path_component(const char path[], char buf[])
{
	const char *const last = get_last_path_component(path);
	snprintf(buf, until_first(last, '/') - last + 1, "%s", last);
}

int
shellout(const char command[], int pause, int use_term_multiplexer)
{
	char *cmd;
	int result;
	int ec;

	if(pause > 0 && command != NULL && ends_with(command, "&"))
	{
		pause = -1;
	}

	cmd = gen_shell_cmd(command, pause > 0, use_term_multiplexer);

	endwin();

	/* Need to use setenv instead of getcwd for a symlink directory */
	env_set("PWD", curr_view->curr_dir);

	ec = vifm_system(cmd);
	/* No WIFEXITED(ec) check here, since vifm_system(...) shouldn't return until
	 * subprocess exited. */
	result = WEXITSTATUS(ec);

	if(result != 0 && pause < 0)
	{
		LOG_ERROR_MSG("Subprocess (%s) exit code: %d (0x%x); status = 0x%x", cmd,
				result, result, ec);
		pause_shell();
	}

	free(cmd);

	/* Force views update. */
	ui_view_schedule_reload(&lwin);
	ui_view_schedule_reload(&rwin);

	recover_after_shellout();

	if(!curr_stats.skip_shellout_redraw)
	{
		/* Redraw to handle resizing of terminal that we could have missed. */
		curr_stats.need_update = UT_FULL;
	}

	curs_set(FALSE);

	return result;
}

/* Composes shell command to run basing on parameters for execution.  NULL cmd
 * parameter opens shell.  Returns a newly allocated string, which should be
 * freed by the caller. */
static char *
gen_shell_cmd(const char cmd[], int pause, int use_term_multiplexer)
{
	char *shell_cmd = NULL;

	if(cmd != NULL)
	{
		if(use_term_multiplexer && curr_stats.term_multiplexer != TM_NONE)
		{
			shell_cmd = gen_term_multiplexer_cmd(cmd, pause);
		}
		else
		{
			shell_cmd = gen_normal_cmd(cmd, pause);
		}
	}
	else if(use_term_multiplexer)
	{
		shell_cmd = gen_term_multiplexer_run_cmd();
	}

	if(shell_cmd == NULL)
	{
		shell_cmd = strdup(cfg.shell);
	}

	return shell_cmd;
}

/* Composes command to be run using terminal multiplexer.  Returns newly
 * allocated string that should be freed by the caller. */
static char *
gen_term_multiplexer_cmd(const char cmd[], int pause)
{
	char *title_arg;
	char *escaped_sh;
	char *raw_shell_cmd;
	char *escaped_shell_cmd;
	char *shell_cmd = NULL;

	if(curr_stats.term_multiplexer != TM_TMUX &&
			curr_stats.term_multiplexer != TM_SCREEN)
	{
		assert(0 && "Unexpected active terminal multiplexer value.");
		return NULL;
	}

	escaped_sh = escape_filename(cfg.shell, 0);

	title_arg = gen_term_multiplexer_title_arg(cmd);

	raw_shell_cmd = format_str("%s%s", cmd, pause ? PAUSE_STR : "");
	escaped_shell_cmd = escape_filename(raw_shell_cmd, 0);

	if(curr_stats.term_multiplexer == TM_TMUX)
	{
		char *const arg = format_str("%s -c %s", escaped_sh, escaped_shell_cmd);
		char *const escaped_arg = escape_filename(arg, 0);

		shell_cmd = format_str("tmux new-window %s %s", title_arg, escaped_arg);

		free(escaped_arg);
		free(arg);
	}
	else if(curr_stats.term_multiplexer == TM_SCREEN)
	{
		set_pwd_in_screen(curr_view->curr_dir);

		shell_cmd = format_str("screen %s %s -c %s", title_arg, escaped_sh,
				escaped_shell_cmd);
	}
	else
	{
		assert(0 && "Unsupported terminal multiplexer type.");
	}

	free(escaped_shell_cmd);
	free(raw_shell_cmd);
	free(title_arg);
	free(escaped_sh);

	return shell_cmd;
}

/* Composes title for window of a terminal multiplexer from a command.  Returns
 * newly allocated string that should be freed by the caller. */
static char *
gen_term_multiplexer_title_arg(const char cmd[])
{
	int bg;
	const char *const vicmd = cfg_get_vicmd(&bg);
	const char *const visubcmd = strstr(cmd, vicmd);
	char *command_name = NULL;
	const char *title;
	char *title_arg;

	if(visubcmd != NULL)
	{
		title = skip_whitespace(visubcmd + strlen(vicmd) + 1);
	}
	else
	{
		char *const separator = strchr(cmd, ' ');
		if(separator != NULL)
		{
			*separator = '\0';
			command_name = strdup(cmd);
			*separator = ' ';
		}
		title = command_name;
	}

	if(is_null_or_empty(title))
	{
		title_arg = strdup("");
	}
	else
	{
		const char opt_c = (curr_stats.term_multiplexer == TM_SCREEN) ? 't' : 'n';
		char *const escaped_title = escape_filename(title, 0);
		title_arg = format_str("-%c %s", opt_c, escaped_title);
		free(escaped_title);
	}

	free(command_name);

	return title_arg;
}

/* Composes command to be run without terminal multiplexer.  Returns a newly
 * allocated string, which should be freed by the caller. */
static char *
gen_normal_cmd(const char cmd[], int pause)
{
	if(pause)
	{
		const char *cmd_with_pause_fmt;

#ifdef _WIN32
		if(curr_stats.shell_type == ST_CMD)
		{
			cmd_with_pause_fmt = "%s" PAUSE_STR;
		}
		else
#endif
		{
			cmd_with_pause_fmt = "%s; " PAUSE_CMD;
		}

		return format_str(cmd_with_pause_fmt, cmd);
	}
	else
	{
		return strdup(cmd);
	}
}

/* Composes shell command to run active terminal multiplexer.  Returns a newly
 * allocated string, which should be freed by the caller. */
static char *
gen_term_multiplexer_run_cmd(void)
{
	char *shell_cmd = NULL;

	if(curr_stats.term_multiplexer == TM_SCREEN)
	{
		set_pwd_in_screen(curr_view->curr_dir);

		shell_cmd = strdup("screen");
	}
	else if(curr_stats.term_multiplexer == TM_TMUX)
	{
		shell_cmd = strdup("tmux new-window");
	}
	else
	{
		assert(0 && "Unexpected active terminal multiplexer value.");
	}

	return shell_cmd;
}

/* Changes $PWD in running GNU/screen session to the specified path.  Needed for
 * symlink directories and sshfs mounts. */
static void
set_pwd_in_screen(const char path[])
{
	char *const escaped_dir = escape_filename(path, 0);
	char *const set_pwd = format_str("screen -X setenv PWD %s", escaped_dir);

	(void)vifm_system(set_pwd);

	free(set_pwd);
	free(escaped_dir);
}

void
output_to_nowhere(const char cmd[])
{
	FILE *file, *err;

	if(background_and_capture((char *)cmd, &file, &err) == (pid_t)-1)
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
	char *const typed_fname = get_typed_current_fname(view);
	assoc_records_t ft = ft_get_all_programs(typed_fname);
	assoc_records_t magic = get_magic_handlers(typed_fname);
	free(typed_fname);

	if(try_run_with_filetype(view, ft, beginning, background))
	{
		ft_assoc_records_free(&ft);
		return 0;
	}

	ft_assoc_records_free(&ft);

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
