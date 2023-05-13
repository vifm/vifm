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

#include <sys/stat.h> /* stat */
#ifdef _WIN32
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
#include <stdlib.h> /* EXIT_FAILURE EXIT_SUCCESS free() realloc() */
#include <string.h> /* strcmp() strerror() strrchr() strcat() strstr() strlen()
                       strchr() strdup() strncmp() */

#include "cfg/config.h"
#include "cfg/info.h"
#include "compat/fs_limits.h"
#include "compat/os.h"
#include "int/file_magic.h"
#include "int/fuse.h"
#include "int/path_env.h"
#include "int/vim.h"
#include "lua/vlua.h"
#include "menus/users_menu.h"
#include "modes/dialogs/msg_dialog.h"
#include "modes/view.h"
#include "ui/statusbar.h"
#include "ui/quickview.h"
#include "ui/ui.h"
#include "utils/env.h"
#include "utils/fs.h"
#include "utils/log.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/utils.h"
#include "utils/utf8.h"
#include "background.h"
#include "filelist.h"
#include "filetype.h"
#include "flist_hist.h"
#include "flist_pos.h"
#include "flist_sel.h"
#include "macros.h"
#include "opt_handlers.h"
#include "status.h"
#include "types.h"
#include "vifm.h"

/* Kinds of symbolic link file treatment on file handling. */
typedef enum
{
	FHL_NO_FOLLOW,  /* Don't follow (navigate to instead of navigation inside). */
	FHL_FOLLOW,     /* Follow (end up on the link target, not inside it). */
	FHL_FOLLOW_ALL, /* Follow all the way (end up on final link's target). */
}
FileHandleLink;

static void handle_file(view_t *view, FileHandleExec exec,
		FileHandleLink follow);
static int is_multiselect(view_t *view);
static int is_runnable(const char full_path[], int type, int force_follow);
static int is_executable(const char full_path[], const dir_entry_t *curr,
		int runnable);
#ifdef _WIN32
static void run_win_executable(char full_path[], int elevate);
static int run_win_executable_as_evaluated(const char full_path[]);
#endif
static int selection_is_consistent(view_t *view);
static void execute_file(const char full_path[], int elevate);
static void run_selection(view_t *view, int dont_execute);
static void run_with_defaults(view_t *view);
static void run_selection_separately(view_t *view, int dont_execute);
static int is_multi_run_compat(view_t *view, const char prog_cmd[]);
static void run_explicit_prog(view_t *view, const char prog_spec[], int pause,
		int force_bg);
static void run_implicit_prog(view_t *view, const char prog_spec[], int pause,
		int force_bg);
static void view_current_file(const view_t *view);
static void follow_link(view_t *view, int follow_dirs, int ultimate);
static int cd_to_parent_dir(view_t *view);
static void extract_last_path_component(const char path[], char buf[]);
static char * run_shell_prepare(const char command[], ShellPause pause,
		int use_term_multiplexer, ShellRequester by);
static int run_shell_finish(const char cmd[], const char final_cmd[],
		ShellPause pause, int status);
static void setup_shellout_env(void);
static void cleanup_shellout_env(void);
static char * gen_shell_cmd(const char cmd[], int pause,
		int use_term_multiplexer, ShellRequester *by);
static char * gen_term_multiplexer_cmd(const char cmd[], int pause,
		ShellRequester by);
static char * gen_term_multiplexer_title_arg(const char cmd[]);
static char * gen_normal_cmd(const char cmd[], int pause);
static void set_pwd_in_screen(const char path[]);
static int try_run_with_filetype(view_t *view, const assoc_records_t assocs,
		const char start[], int background);
static void output_to_statusbar(const char cmd[], view_t *view,
		MacroFlags flags);
static int output_to_preview(view_t *view, const char cmd[], MacroFlags flags);
static void run_in_split(const view_t *view, const char cmd[], int vert_split);
static void path_handler(const char line[], void *arg);
static void line_handler(const char line[], void *arg);

/* Name of environment variable used to communicate path to file used to
 * initiate FUSE mounting of directory we're in. */
static const char *const FUSE_FILE_ENVVAR = "VIFM_FUSE_FILE";

void
rn_open(view_t *view, FileHandleExec exec)
{
	handle_file(view, exec, FHL_NO_FOLLOW);
}

void
rn_follow(view_t *view, int ultimate)
{
	if(flist_custom_active(view))
	{
		const dir_entry_t *const curr = get_current_entry(view);
		if(!fentry_is_fake(curr))
		{
			/* Entry might be freed on navigation, so make sure name and origin will
			 * remain available for the call. */
			char *const name = strdup(curr->name);
			char *const origin = strdup(curr->origin);
			navigate_to_file(view, origin, name, 0);
			free(origin);
			free(name);
		}
		return;
	}

	handle_file(view, FHE_RUN, ultimate ? FHL_FOLLOW_ALL : FHL_FOLLOW);
}

static void
handle_file(view_t *view, FileHandleExec exec, FileHandleLink follow)
{
	char full_path[PATH_MAX + 1];
	const dir_entry_t *const curr = get_current_entry(view);

	int user_selection = !view->pending_marking;
	flist_set_marking(view, 1);

	if(fentry_is_fake(curr))
	{
		return;
	}

	get_full_path_of(curr, sizeof(full_path), full_path);

	int could_enter_entry = (curr->type != FT_LINK || follow == FHL_NO_FOLLOW);
	int selected_entry = (curr->marked && (!user_selection || curr->selected));
	if(!selected_entry && could_enter_entry)
	{
		int dir_like_entry = (fentry_is_dir(curr) || is_unc_root(view->curr_dir));
		if(dir_like_entry)
		{
			(void)rn_enter_dir(view);
			return;
		}
	}

	int multiselect = is_multiselect(view);
	int runnable = is_runnable(full_path, curr->type, follow != FHL_NO_FOLLOW);
	int executable = (exec != FHE_NO_RUN)
	              && cfg.auto_execute
	              && is_executable(full_path, curr, runnable);

	if(stats_file_choose_action_set() && (multiselect || runnable || executable))
	{
		/* Reuse marking second time. */
		view->pending_marking = 1;
		/* The call below does not return. */
		vifm_choose_files(view, 0, NULL);
	}

	if(executable && !fentry_is_dir(curr))
	{
		execute_file(full_path, exec == FHE_ELEVATE_AND_RUN);
	}
	else if(multiselect || runnable)
	{
		run_selection(view, exec == FHE_NO_RUN);
	}
	else if(curr->type == FT_LINK || is_shortcut(curr->name))
	{
		follow_link(view, follow != FHL_NO_FOLLOW, follow == FHL_FOLLOW_ALL);
	}
}

/* Checks whether marking covers multiple files.  Returns non-zero if so. */
static int
is_multiselect(view_t *view)
{
	/* Checking for at least 2 files in the marking. */
	dir_entry_t *entry = NULL;
	return iter_marked_entries(view, &entry)
	    && iter_marked_entries(view, &entry);
}

/* Returns non-zero if file can be executed or it's a link to a directory (it
 * can be entered), otherwise zero is returned. */
static int
is_runnable(const char full_path[], int type, int force_follow)
{
	if(type == FT_LINK)
	{
		return (!force_follow && !cfg.follow_links);
	}

	if(type == FT_REG)
	{
		return (!force_follow || !is_shortcut(full_path));
	}

	return (type == FT_EXEC || type == FT_DIR);
}

/* Returns non-zero if file can be executed, otherwise zero is returned. */
static int
is_executable(const char full_path[], const dir_entry_t *curr, int runnable)
{
	int executable = (curr->type == FT_EXEC);
#ifndef _WIN32
	if(!executable)
	{
		/* XXX: why "runnable" is here?  Aren't the checks excessive? */
		executable = runnable
		          && (os_access(full_path, X_OK) == 0)
		          && S_ISEXE(curr->mode);
	}
#endif
	return executable;
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
		const int error = win_exec_cmd(full_path, full_path, &returned_exit_code);
		if(error != 0 && !returned_exit_code)
		{
			if(error == ERROR_ELEVATION_REQUIRED && is_vista_and_above())
			{
				const int user_response = prompt_msg("Program running error",
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
selection_is_consistent(view_t *view)
{
	int files = 0, dirs = 0;
	dir_entry_t *entry = NULL;
	while(iter_marked_entries(view, &entry))
	{
		if(fentry_is_dir(entry))
		{
			++dirs;
		}
		else
		{
			++files;
		}
	}
	return (dirs == 0 || files == 0);
}

/* Executes file, specified by the full_path.  Changes type of slashes on
 * Windows. */
static void
execute_file(const char full_path[], int elevate)
{
#ifndef _WIN32
	char *const escaped = shell_arg_escape(full_path, curr_stats.shell_type);
	rn_shell(escaped, PAUSE_ALWAYS, 1, SHELL_BY_APP);
	free(escaped);
#else
	char *copy = strdup(full_path);
	internal_to_system_slashes(copy);

	char *escaped = shell_arg_escape(copy, curr_stats.shell_type);
	free(copy);

	run_win_executable(escaped, elevate);
	free(escaped);
#endif
}

/* Tries to run selection displaying error message on file type
 * inconsistency. */
static void
run_selection(view_t *view, int dont_execute)
{
	if(!selection_is_consistent(view))
	{
		show_error_msg("Selection error",
				"Selection cannot contain files and directories at the same time");
		return;
	}

	char *typed_fname = get_typed_entry_fpath(get_current_entry(view));
	const char *common_prog_cmd = ft_get_program(typed_fname);
	free(typed_fname);

	int can_multi_run = (is_multi_run_compat(view, common_prog_cmd) != 0);
	int files_without_handler = (common_prog_cmd == NULL);
	int identical_handlers = 1;
	int nentries = 0;

	dir_entry_t *entry = NULL;
	while(iter_marked_entries(view, &entry))
	{
		++nentries;

		if(!path_exists_at(entry->origin, entry->name, DEREF))
		{
			show_error_msgf("Broken Link", "Destination of \"%s\" link doesn't exist",
					entry->name);
			return;
		}

		char *typed_fname = get_typed_entry_fpath(entry);
		const char *entry_prog_cmd = ft_get_program(typed_fname);
		free(typed_fname);

		if(entry_prog_cmd == NULL)
		{
			++files_without_handler;
			continue;
		}

		can_multi_run &= (is_multi_run_compat(view, entry_prog_cmd) != 0);
		if(common_prog_cmd == NULL)
		{
			common_prog_cmd = entry_prog_cmd;
		}
		else if(strcmp(entry_prog_cmd, common_prog_cmd) != 0)
		{
			identical_handlers = 0;
		}
	}

	can_multi_run &= (nentries > 1);

	if(files_without_handler > 0)
	{
		run_with_defaults(view);
	}
	else if(can_multi_run)
	{
		run_selection_separately(view, dont_execute);
	}
	else if(identical_handlers)
	{
		rn_open_with(view, common_prog_cmd, dont_execute, 0);
	}
	else
	{
		show_error_msg("Run error", "Handlers of selected files are "
		                            "incompatible.");
	}
}

/* Runs current file entry of the view in a generic way (entering directories
 * and opening files in editors). */
static void
run_with_defaults(view_t *view)
{
	if(get_current_entry(view)->type == FT_DIR)
	{
		(void)rn_enter_dir(view);
		return;
	}

	/* Reuse marking second time. */
	view->pending_marking = 1;
	if(vim_edit_marking(view) != 0)
	{
		show_error_msg("Running error", "Can't edit selection");
	}
}

/* Runs each of selected file entries of the view individually. */
static void
run_selection_separately(view_t *view, int dont_execute)
{
	const int pos = view->list_pos;

	dir_entry_t *entry = NULL;
	while(iter_marked_entries(view, &entry))
	{
		char *typed_fname;
		const char *entry_prog_cmd;

		typed_fname = get_typed_entry_fpath(entry);
		entry_prog_cmd = ft_get_program(typed_fname);
		free(typed_fname);

		view->list_pos = entry_to_pos(view, entry);
		rn_open_with(view, entry_prog_cmd, dont_execute, 0);
	}

	view->list_pos = pos;
}

/* Checks whether command is compatible with firing multiple file handlers for a
 * set of selected files.  Returns non-zero if so, otherwise zero is
 * returned. */
static int
is_multi_run_compat(view_t *view, const char prog_cmd[])
{
	size_t len;
	if(prog_cmd == NULL)
		return 0;
	if((len = strlen(prog_cmd)) == 0)
		return 0;
	/* XXX: should the check be for " &" and not just "&"? */
	if(prog_cmd[len - 1] != '&')
		return 0;
	if(strstr(prog_cmd, "%f") != NULL || strstr(prog_cmd, "%F") != NULL)
		return 0;
	if(strstr(prog_cmd, "%c") == NULL && strstr(prog_cmd, "%C") == NULL)
		return 0;
	return 1;
}

void
rn_open_with(view_t *view, const char prog_spec[], int dont_execute,
		int force_bg)
{
	const dir_entry_t *const curr = get_current_entry(view);
	const int pause = skip_prefix(&prog_spec, "!!");

	if(!path_exists_at(curr->origin, curr->name, DEREF))
	{
		show_error_msg("Access Error", "File doesn't exist.");
		return;
	}

	if(vlua_handler_cmd(curr_stats.vlua, prog_spec))
	{
		vlua_open_file(curr_stats.vlua, prog_spec, curr);
		return;
	}

	if(fuse_is_mount_string(prog_spec))
	{
		if(dont_execute)
		{
			view_current_file(view);
		}
		else
		{
			fuse_try_mount(view, prog_spec);
		}
	}
	else if(strcmp(prog_spec, VIFM_PSEUDO_CMD) == 0)
	{
		(void)rn_enter_dir(view);
	}
	else if(strchr(prog_spec, '%') != NULL)
	{
		run_explicit_prog(view, prog_spec, pause, force_bg);
	}
	else
	{
		run_implicit_prog(view, prog_spec, pause, force_bg);
	}
}

/* Executes current file of the current view by program specification that
 * includes at least one macro. */
static void
run_explicit_prog(view_t *view, const char prog_spec[], int pause, int force_bg)
{
	MacroFlags flags;
	char *const cmd = ma_expand(prog_spec, NULL, &flags, MER_SHELL_OP);

	const ShellPause pause_shell = (pause ? PAUSE_ALWAYS : PAUSE_ON_ERROR);

	int bg = cut_suffix(cmd, " &");
	bg = !pause && (bg || force_bg);

	int save_msg = 0;
	if(rn_ext(view, cmd, prog_spec, flags, bg, &save_msg) != 0)
	{
		if(save_msg)
		{
			curr_stats.save_msg = 1;
		}
	}
	else if(bg)
	{
		assert(ma_flags_missing(flags, MF_IGNORE) && "This case is for rn_ext()");
		rn_start_bg_command(view, cmd, flags);
	}
	else if(ma_flags_present(flags, MF_PIPE_FILE_LIST) ||
			ma_flags_present(flags, MF_PIPE_FILE_LIST_Z))
	{
		rn_pipe(cmd, view, flags, pause_shell);
	}
	else
	{
		const int use_term_multiplexer = ma_flags_missing(flags, MF_NO_TERM_MUX);
		(void)rn_shell(cmd, pause_shell, use_term_multiplexer, SHELL_BY_USER);
	}

	free(cmd);
}

/* Executes current file of the view by program specification that does not
 * include any macros (hence file name is appended implicitly. */
static void
run_implicit_prog(view_t *view, const char prog_spec[], int pause, int force_bg)
{
	int bg;
	char cmd[NAME_MAX + 1 + NAME_MAX + 1];
	const char *name_macro;
	char *file_name;
	char spec[strlen(prog_spec) + 1U];

	strcpy(spec, prog_spec);
	bg = cut_suffix(spec, " &") || force_bg;

	if(curr_stats.shell_type == ST_CMD || curr_stats.shell_type == ST_YORI)
	{
		name_macro = (view == curr_view) ? "%\"c" : "%\"C";
	}
	else
	{
		name_macro = (view == curr_view) ? "%c" : "%C";
	}

	file_name = ma_expand(name_macro, NULL, NULL, MER_SHELL_OP);
	snprintf(cmd, sizeof(cmd), "%s %s", spec, file_name);
	free(file_name);

	if(bg)
	{
		(void)bg_run_external(cmd, 0, SHELL_BY_USER, NULL);
	}
	else
	{
		(void)rn_shell(cmd, pause ? PAUSE_ALWAYS : PAUSE_ON_ERROR, 1,
				SHELL_BY_USER);
	}
}

/* Opens file under the cursor in the viewer. */
static void
view_current_file(const view_t *view)
{
	char full_path[PATH_MAX + 1];
	get_current_full_path(view, sizeof(full_path), full_path);
	(void)vim_view_file(full_path, -1, -1, 1);
}

/* Resolves link target and either navigates inside directory the link points to
 * or navigates to directory where target is located placing cursor at it (the
 * follow_dirs flag controls the behaviour). */
static void
follow_link(view_t *view, int follow_dirs, int ultimate)
{
	char *dir, *file;
	char linkto[PATH_MAX + NAME_MAX + 1];
	const dir_entry_t *const curr = get_current_entry(view);

	char full_path[PATH_MAX + 1];
	get_full_path_of(curr, sizeof(full_path), full_path);

	int resolve_one_level = !ultimate;

	if(ultimate)
	{
		if(os_realpath(full_path, linkto) != linkto)
		{
			show_error_msg("Error", "Can't resolve the link.");
			return;
		}

#ifdef _WIN32
		/* This might be a Windows shortcut which aren't resolved by
		 * os_realpath(). */
		resolve_one_level = paths_are_equal(full_path, linkto);
#endif
	}

	if(resolve_one_level)
	{
		/* We resolve origin to a real path because relative symbolic links are
		 * relative to real location of the link.  Can't simply do realpath() on
		 * full path as it would resolve symbolic links recursively, while we want
		 * to follow only one level deep. */

		char origin_real[PATH_MAX + 1];
		if(os_realpath(curr->origin, origin_real) != origin_real)
		{
			show_error_msg("Error", "Can't resolve origin of the link.");
			return;
		}

		if(get_link_target_abs(full_path, origin_real, linkto, sizeof(linkto)) != 0)
		{
			show_error_msg("Error", "Can't read link.");
			return;
		}
	}

	if(!path_exists(linkto, NODEREF))
	{
		show_error_msg("Broken Link",
				"Can't access link destination.  It might be broken.");
		return;
	}

	chosp(linkto);

	if(is_dir(linkto) && !follow_dirs)
	{
		dir = strdup(curr->name);
		file = NULL;
	}
	else
	{
		dir = strdup(linkto);
		remove_last_path_component(dir);

		file = get_last_path_component(linkto);
	}

	if(dir[0] != '\0' && file != NULL)
	{
		navigate_to_file(view, dir, file, 1);
	}
	else if(dir[0] != '\0')
	{
		navigate_to(view, dir);
	}
	else if(file != NULL)
	{
		fpos_ensure_selected(view, file);
	}

	free(dir);
}

int
rn_enter_dir(view_t *view)
{
	dir_entry_t *const curr = get_current_entry(view);

	if(is_parent_dir(curr->name) && !curr->owns_origin)
	{
		rn_leave(view, 1);
		return 0;
	}

	char full_path[PATH_MAX + 1];
	get_full_path_of(curr, sizeof(full_path), full_path);
	if(!cd_is_possible(full_path))
	{
		return 1;
	}

	curr_stats.ch_pos = (cfg_ch_pos_on(CHPOS_ENTER) ? 1 : 0);
	int result = navigate_to(view, full_path);
	curr_stats.ch_pos = 1;

	return result;
}

void
rn_leave(view_t *view, int levels)
{
	/* Do not save intermediate directories in directory history. */
	curr_stats.drop_new_dir_hist = 1;

	while(levels-- > 0)
	{
		if(cd_to_parent_dir(view) != 0)
		{
			break;
		}
	}

	curr_stats.drop_new_dir_hist = 0;
	flist_hist_save(view);
}

/* Goes one directory up from current location.  Returns zero unless it won't
 * make sense to continue going up (like on error or reaching root). */
static int
cd_to_parent_dir(view_t *view)
{
	char dir_name[strlen(view->curr_dir) + 1];
	int ret;

	/* Return to original directory from custom view. */
	if(flist_custom_active(view))
	{
		navigate_to(view, view->custom.orig_dir);
		return 0;
	}

	/* Do nothing in root. */
	if(is_root_dir(view->curr_dir))
	{
		return 1;
	}

	dir_name[0] = '\0';
	extract_last_path_component(view->curr_dir, dir_name);

	ret = change_directory(view, "../");
	if(ret == -1)
	{
		return 1;
	}

	if(ret == 0)
	{
		/* XXX: we put cursor at one position and then move it.  Ideally it would
		 *      be set where it should be right away. */
		load_dir_list(view, 0);
		fpos_set_pos(view, fpos_find_by_name(view, dir_name));
	}
	return 0;
}

/* Extracts last part of the path into buf.  Assumes that size of the buf is
 * large enough. */
static void
extract_last_path_component(const char path[], char buf[])
{
	const char *const last = get_last_path_component(path);
	copy_str(buf, until_first(last, '/') - last + 1, last);
}

int
rn_shell(const char command[], ShellPause pause, int use_term_multiplexer,
		ShellRequester by)
{
	char *cmd = run_shell_prepare(command, pause, use_term_multiplexer, by);
	int status = vifm_system(cmd, by);
	int exit_code = run_shell_finish(command, cmd, pause, status);

	free(cmd);
	return exit_code;
}

int
rn_pipe(const char command[], view_t *view, MacroFlags flags, ShellPause pause)
{
	assert((ma_flags_present(flags, MF_PIPE_FILE_LIST)
			|| ma_flags_present(flags, MF_PIPE_FILE_LIST_Z))
			&& "rn_pipe() must be called only when piping is requested");

	FILE *input_tmp = make_in_file(view, flags);
	char *cmd = run_shell_prepare(command, pause, 0, SHELL_BY_USER);

	int status = vifm_system_input(cmd, input_tmp, SHELL_BY_USER);
	int exit_code = run_shell_finish(command, cmd, pause, status);

	free(cmd);
	fclose(input_tmp);

	return exit_code;
}

/* Prepares to run a command.  Returns actual command to execute, which should
 * be freed by the caller. */
static char *
run_shell_prepare(const char command[], ShellPause pause,
		int use_term_multiplexer, ShellRequester by)
{
	/* Shutdown UI at this point, where $PATH isn't cleared. */
	ui_shutdown();

	if(command == NULL)
	{
		command = env_get_def("SHELL", cfg.shell);

		/* Run shell with clean $PATH environment variable. */
		load_clean_path_env();
	}

	setup_shellout_env();

	return gen_shell_cmd(command, pause == PAUSE_ALWAYS, use_term_multiplexer,
			&by);
}

/* Handles command running result.  Returns exit code. */
static int
run_shell_finish(const char cmd[], const char final_cmd[], ShellPause pause,
		int status)
{
	cleanup_shellout_env();

	int exit_code = status_to_exit_code(status);

	if(pause == PAUSE_ALWAYS && final_cmd != NULL && ends_with(final_cmd, "&"))
	{
		pause = PAUSE_ON_ERROR;
	}

	if(exit_code != 0 && pause == PAUSE_ON_ERROR)
	{
		LOG_ERROR_MSG("Subprocess (%s) exit code: %d (0x%x); status = 0x%x",
				final_cmd, exit_code, exit_code, status);
		pause_shell();
	}

	/* Force updates of views that don't have associated watchers. */
	if(!flist_is_fs_backed(&lwin))
	{
		ui_view_schedule_reload(&lwin);
	}
	if(!flist_is_fs_backed(&rwin))
	{
		ui_view_schedule_reload(&rwin);
	}

	recover_after_shellout();

	if(!curr_stats.skip_shellout_redraw)
	{
		/* Redraw to handle resizing of terminal that we could have missed. */
		stats_redraw_later();
	}

	ui_set_cursor(/*visibility=*/0);

	if(cmd == NULL)
	{
		load_real_path_env();
	}

	return exit_code;
}

/* Configures environment variables before shellout.  Should be used in pair
 * with cleanup_shellout_env(). */
static void
setup_shellout_env(void)
{
	const char *mount_file;
	const char *term_multiplexer_fmt;
	char *escaped_path;
	char *cmd;

	/* Need to use internal value instead of getcwd() for a symlink directory. */
	env_set("PWD", curr_view->curr_dir);

	mount_file = fuse_get_mount_file(curr_view->curr_dir);
	if(mount_file == NULL)
	{
		env_remove(FUSE_FILE_ENVVAR);
		return;
	}

	env_set(FUSE_FILE_ENVVAR, mount_file);

	switch(curr_stats.term_multiplexer)
	{
		case TM_TMUX:   term_multiplexer_fmt = "tmux set-environment %s %s"; break;
		case TM_SCREEN: term_multiplexer_fmt = "screen -X setenv %s %s"; break;

		default:
			return;
	}

	escaped_path = shell_arg_escape(mount_file, curr_stats.shell_type);
	cmd = format_str(term_multiplexer_fmt, FUSE_FILE_ENVVAR, escaped_path);
	(void)vifm_system(cmd, SHELL_BY_APP);
	free(cmd);
	free(escaped_path);
}

/* Cleans up some environment changes made by setup_shellout_env() after command
 * execution ends. */
static void
cleanup_shellout_env(void)
{
	const char *term_multiplexer_fmt;
	char *cmd;

	switch(curr_stats.term_multiplexer)
	{
		case TM_TMUX:   term_multiplexer_fmt = "tmux set-environment -u %s"; break;
		case TM_SCREEN: term_multiplexer_fmt = "screen -X unsetenv %s"; break;

		default:
			return;
	}

	cmd = format_str(term_multiplexer_fmt, FUSE_FILE_ENVVAR);
	(void)vifm_system(cmd, SHELL_BY_APP);
	free(cmd);
}

/* Composes shell command to run based on parameters for execution.  Returns a
 * newly allocated string, which should be freed by the caller. */
static char *
gen_shell_cmd(const char cmd[], int pause, int use_term_multiplexer,
		ShellRequester *by)
{
	char *shell_cmd;

	if(use_term_multiplexer && curr_stats.term_multiplexer != TM_NONE)
	{
		shell_cmd = gen_term_multiplexer_cmd(cmd, pause, *by);
		/* User shell settings were taken into account in command for multiplexer,
		 * don't use them to invoke the multiplexer itself. */
		*by = SHELL_BY_APP;
	}
	else
	{
		shell_cmd = gen_normal_cmd(cmd, pause);
	}

	return shell_cmd;
}

/* Composes command to be run using terminal multiplexer.  Returns newly
 * allocated string that should be freed by the caller. */
static char *
gen_term_multiplexer_cmd(const char cmd[], int pause, ShellRequester by)
{
	char *title_arg;
	char *raw_shell_cmd;
	char *escaped_shell_cmd;
	char *shell_cmd = NULL;

	if(curr_stats.term_multiplexer != TM_TMUX &&
			curr_stats.term_multiplexer != TM_SCREEN)
	{
		assert(0 && "Unexpected active terminal multiplexer value.");
		return NULL;
	}

	title_arg = gen_term_multiplexer_title_arg(cmd);

	raw_shell_cmd = format_str("%s%s", cmd, pause ? PAUSE_STR : "");
	escaped_shell_cmd = shell_arg_escape(raw_shell_cmd, curr_stats.shell_type);

	const char *sh_flag = (by == SHELL_BY_USER ? cfg.shell_cmd_flag : "-c");

	if(curr_stats.term_multiplexer == TM_TMUX)
	{
		char *const arg = format_str("%s %s %s", cfg.shell, sh_flag,
				escaped_shell_cmd);
		char *const escaped_arg = shell_arg_escape(arg, curr_stats.shell_type);

		shell_cmd = format_str("tmux new-window %s %s", title_arg, escaped_arg);

		free(escaped_arg);
		free(arg);
	}
	else if(curr_stats.term_multiplexer == TM_SCREEN)
	{
		set_pwd_in_screen(flist_get_dir(curr_view));

		shell_cmd = format_str("screen %s %s %s %s", title_arg, cfg.shell, sh_flag,
				escaped_shell_cmd);
	}
	else
	{
		assert(0 && "Unsupported terminal multiplexer type.");
	}

	free(escaped_shell_cmd);
	free(raw_shell_cmd);
	free(title_arg);

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
		if(cfg.short_term_mux_titles)
		{
			title = get_last_path_component(title);
		}
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
		char *const escaped_title = shell_arg_escape(title, curr_stats.shell_type);
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

		if(curr_stats.shell_type == ST_CMD || curr_stats.shell_type == ST_YORI)
		{
			cmd_with_pause_fmt = "%s" PAUSE_STR;
		}
		else if(curr_stats.shell_type == ST_PS)
		{
			/* TODO: make pausing work. */
			cmd_with_pause_fmt = "%s";
		}
		else
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

/* Changes $PWD in running GNU/screen session to the specified path.  Needed for
 * symlink directories and sshfs mounts. */
static void
set_pwd_in_screen(const char path[])
{
	char *const escaped_dir = shell_arg_escape(path, curr_stats.shell_type);
	char *const set_pwd = format_str("screen -X setenv PWD %s", escaped_dir);

	(void)vifm_system(set_pwd, SHELL_BY_APP);

	free(set_pwd);
	free(escaped_dir);
}

int
rn_open_with_match(view_t *view, const char beginning[], int background)
{
	dir_entry_t *const curr = get_current_entry(view);
	assoc_records_t ft, magic;
	char *typed_fname;

	if(fentry_is_fake(curr))
	{
		return 1;
	}

	typed_fname = get_typed_entry_fpath(curr);
	ft = ft_get_all_programs(typed_fname);
	magic = get_magic_handlers(typed_fname);
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
try_run_with_filetype(view_t *view, const assoc_records_t assocs,
		const char start[], int background)
{
	const size_t len = strlen(start);
	int i;
	for(i = 0; i < assocs.count; i++)
	{
		if(strncmp(assocs.list[i].command, start, len) == 0)
		{
			rn_open_with(view, assocs.list[i].command, 0, background);
			return 1;
		}
	}
	return 0;
}

int
rn_ext(view_t *view, const char cmd[], const char title[], MacroFlags flags,
		int bg, int *save_msg)
{
	if(bg && (ma_flags_missing(flags, MF_NONE) &&
	          ma_flags_missing(flags, MF_NO_TERM_MUX) &&
	          ma_flags_missing(flags, MF_IGNORE) &&
	          ma_flags_missing(flags, MF_PIPE_FILE_LIST) &&
	          ma_flags_missing(flags, MF_PIPE_FILE_LIST_Z)))
	{
		ui_sb_errf("\"%s\" macro can't be combined with \" &\"",
				ma_flags_to_str(flags));
		*save_msg = 1;
		return -1;
	}

	if(ma_flags_present(flags, MF_STATUSBAR_OUTPUT))
	{
		output_to_statusbar(cmd, view, flags);
		*save_msg = 1;
		return -1;
	}
	else if(ma_flags_present(flags, MF_PREVIEW_OUTPUT))
	{
		view_t *other = (view == curr_view ? other_view : curr_view);
		*save_msg = output_to_preview(other, cmd, flags);
		return -1;
	}
	else if(ma_flags_present(flags, MF_IGNORE))
	{
		/* TODO: consider merging with rn_start_bg_command(). */

		const int supply_input = ma_flags_present(flags, MF_PIPE_FILE_LIST)
		                      || ma_flags_present(flags, MF_PIPE_FILE_LIST_Z);
		FILE *input = NULL;
		FILE **input_ptr = (supply_input ? &input : NULL);
		int error = 0;

		setup_shellout_env();
		if(bg_run_external(cmd, 1, SHELL_BY_USER, input_ptr) != 0)
		{
			error = 1;
		}
		cleanup_shellout_env();

		if(input != NULL)
		{
			const int null_sep = ma_flags_present(flags, MF_PIPE_FILE_LIST_Z);
			write_marked_paths(input, view, null_sep);
			fclose(input);
		}

		if(error)
		{
			show_error_msgf("Trouble running command", "Unable to run: %s", cmd);
		}
		return -1;
	}
	else if(ma_flags_present(flags, MF_MENU_OUTPUT) ||
			ma_flags_present(flags, MF_MENU_NAV_OUTPUT))
	{
		setup_shellout_env();
		*save_msg = show_user_menu(view, cmd, title, flags) != 0;
		cleanup_shellout_env();
	}
	else if((ma_flags_present(flags, MF_SPLIT) ||
	         ma_flags_present(flags, MF_SPLIT_VERT)) &&
	        curr_stats.term_multiplexer != TM_NONE)
	{
		const int vert_split = ma_flags_present(flags, MF_SPLIT_VERT);
		run_in_split(view, cmd, vert_split);
	}
	else if(ma_flags_present(flags, MF_CUSTOMVIEW_OUTPUT) ||
	        ma_flags_present(flags, MF_VERYCUSTOMVIEW_OUTPUT) ||
	        ma_flags_present(flags, MF_CUSTOMVIEW_IOUTPUT) ||
	        ma_flags_present(flags, MF_VERYCUSTOMVIEW_IOUTPUT))
	{
		rn_for_flist(view, cmd, title, /*user_sh=*/1, flags);
	}
	else
	{
		return 0;
	}
	return 1;
}

/* Executes the cmd and displays its output on the status bar. */
static void
output_to_statusbar(const char cmd[], view_t *view, MacroFlags flags)
{
	FILE *file;
	char buf[2048];
	char *lines;
	size_t len;
	FILE *input_tmp = make_in_file(view, flags);

	setup_shellout_env();
	int error = 0;
	if(bg_run_and_capture((char *)cmd, 1, input_tmp, &file, /*err=*/NULL) ==
			(pid_t)-1)
	{
		error = 1;
	}
	cleanup_shellout_env();

	if(input_tmp != NULL)
	{
		fclose(input_tmp);
	}

	if(error)
	{
		show_error_msgf("Trouble running command", "Unable to run: %s", cmd);
		return;
	}

	lines = NULL;
	len = 0;
	/* XXX: the loop can potentially never end if error pipe gets filled, might
	 *      need to redirect stderr to /dev/null instead of opening and not using
	 *      it. */
	while(fgets(buf, sizeof(buf), file) == buf)
	{
		char *p;

		chomp(buf);
		p = realloc(lines, len + 1 + strlen(buf) + 1);
		if(p != NULL)
		{
			lines = p;
			len += sprintf(lines + len, "%s%s", (len == 0) ? "": "\n", buf);
		}
	}

	fclose(file);

	ui_sb_msg((lines == NULL) ? "" : lines);
	free(lines);
}

/* Runs the command and captures its output into detached preview.  Returns new
 * value for the save_msg flag. */
static int
output_to_preview(view_t *view, const char cmd[], MacroFlags flags)
{
	if(qv_ensure_is_shown() != 0)
	{
		return 1;
	}

	modview_detached_make(view, cmd, flags);
	return 0;
}

/* Runs the cmd in a split window of terminal multiplexer.  Runs shell, if cmd
 * is NULL. */
static void
run_in_split(const view_t *view, const char cmd[], int vert_split)
{
	char *const escaped_cmd = (cmd == NULL)
	                        ? strdup(cfg.shell)
	                        : shell_arg_escape(cmd, curr_stats.shell_type);

	setup_shellout_env();

	if(curr_stats.term_multiplexer == TM_TMUX)
	{
		char cmd[1024];
		snprintf(cmd, sizeof(cmd), "tmux split-window %s %s",
			vert_split ? "-h" : "", escaped_cmd);
		(void)vifm_system(cmd, SHELL_BY_APP);
	}
	else if(curr_stats.term_multiplexer == TM_SCREEN)
	{
		char cmd[1024];

		/* "eval" executes each argument as a separate argument, but escaping rules
		 * are not exactly like in shell, so last command is run separately. */
		char *const escaped_dir =
			shell_arg_escape(flist_get_dir(view), curr_stats.shell_type);
		if(vert_split)
		{
			snprintf(cmd, sizeof(cmd), "screen -X eval chdir\\ %s 'focus right' "
					"split\\ -v 'focus right'", escaped_dir);
		}
		else
		{
			snprintf(cmd, sizeof(cmd), "screen -X eval chdir\\ %s 'focus bottom' "
					"split 'focus bottom'", escaped_dir);
		}
		free(escaped_dir);
		(void)vifm_system(cmd, SHELL_BY_APP);

		snprintf(cmd, sizeof(cmd), "screen -X screen vifm-screen-split %s",
				escaped_cmd);
		(void)vifm_system(cmd, SHELL_BY_APP);
	}
	else
	{
		assert(0 && "Unexpected active terminal multiplexer value.");
	}

	cleanup_shellout_env();

	free(escaped_cmd);
}

void
rn_start_bg_command(view_t *view, const char cmd[], MacroFlags flags)
{
	/* TODO: similar code in rn_ext(). */

	const int supply_input = ma_flags_present(flags, MF_PIPE_FILE_LIST)
	                      || ma_flags_present(flags, MF_PIPE_FILE_LIST_Z);
	FILE *input = NULL;

	bg_run_external(cmd, ma_flags_present(flags, MF_IGNORE), SHELL_BY_USER,
			supply_input ? &input : NULL);

	if(input != NULL)
	{
		const int null_sep = ma_flags_present(flags, MF_PIPE_FILE_LIST_Z);
		write_marked_paths(input, view, null_sep);
		fclose(input);
	}
}

int
rn_for_flist(view_t *view, const char cmd[], const char title[], int user_sh,
		MacroFlags flags)
{
	enum { MAX_TITLE_WIDTH = 80 };

	const int very = ma_flags_present(flags, MF_VERYCUSTOMVIEW_OUTPUT)
	              || ma_flags_present(flags, MF_VERYCUSTOMVIEW_IOUTPUT);
	const int interactive = ma_flags_present(flags, MF_CUSTOMVIEW_IOUTPUT)
	                     || ma_flags_present(flags, MF_VERYCUSTOMVIEW_IOUTPUT);

	/* It makes sense to do escaping before adding ellipses to get a predictable
	 * result. */
	char *escaped_title = escape_unreadable(title);
	char *final_title = right_ellipsis(escaped_title, MAX_TITLE_WIDTH,
			curr_stats.ellipsis);
	free(escaped_title);

	flist_custom_start(view, final_title);
	free(final_title);

	if(interactive && curr_stats.load_stage != 0)
	{
		ui_shutdown();
	}

	FILE *input_tmp = make_in_file(view, flags);

	setup_shellout_env();
	int error = (process_cmd_output("Loading custom view", cmd, input_tmp,
				user_sh, interactive, &path_handler, view) != 0);
	cleanup_shellout_env();

	if(input_tmp != NULL)
	{
		fclose(input_tmp);
	}

	if(error)
	{
		show_error_msgf("Trouble running command", "Unable to run: %s", cmd);
		return 1;
	}

	flist_custom_end(view, very);
	return 0;
}

/* Implements process_cmd_output() callback that loads paths into custom
 * view. */
static void
path_handler(const char line[], void *arg)
{
	view_t *view = arg;
	flist_custom_add_spec(view, line);
}

int
rn_for_lines(view_t *view, const char cmd[], char ***lines, int *nlines,
		MacroFlags flags)
{
	int error;
	strlist_t list = {};
	FILE *input_tmp = make_in_file(view, flags);

	setup_shellout_env();
	error = (process_cmd_output("Loading list", cmd, input_tmp, 1, 0,
				&line_handler, &list) != 0);
	cleanup_shellout_env();

	if(input_tmp != NULL)
	{
		fclose(input_tmp);
	}

	if(error)
	{
		free_string_array(list.items, list.nitems);
		return 1;
	}

	*lines = list.items;
	*nlines = list.nitems;
	return 0;
}

/* Implements process_cmd_output() callback that collects lines into a list. */
static void
line_handler(const char line[], void *arg)
{
	strlist_t *const list = arg;
	list->nitems = add_to_string_array(&list->items, list->nitems, line);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
