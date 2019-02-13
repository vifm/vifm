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

#ifdef _WIN32
#define _WIN32_WINNT 0x0500
#include <windows.h>
#endif

#include "vifm.h"

#include <curses.h>

#include <unistd.h>

#include <errno.h> /* errno */
#include <locale.h> /* setlocale() LC_ALL */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* fprintf() fputs() puts() snprintf() */
#include <stdlib.h> /* EXIT_FAILURE EXIT_SUCCESS exit() srand() system() */
#include <string.h>
#include <time.h> /* time() */

#include "cfg/config.h"
#include "cfg/info.h"
#include "compat/reallocarray.h"
#include "engine/autocmds.h"
#include "engine/parsing.h"
#include "compat/fs_limits.h"
#include "engine/cmds.h"
#include "engine/keys.h"
#include "engine/mode.h"
#include "engine/options.h"
#include "engine/variables.h"
#include "int/fuse.h"
#include "int/path_env.h"
#include "int/term_title.h"
#include "int/vim.h"
#include "modes/dialogs/msg_dialog.h"
#include "modes/cmdline.h"
#include "modes/modes.h"
#include "modes/normal.h"
#include "modes/view.h"
#include "ui/cancellation.h"
#include "ui/color_scheme.h"
#include "ui/quickview.h"
#include "ui/statusbar.h"
#include "ui/tabs.h"
#include "ui/ui.h"
#include "utils/env.h"
#include "utils/fs.h"
#include "utils/log.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "args.h"
#include "background.h"
#include "bmarks.h"
#include "bracket_notation.h"
#include "builtin_functions.h"
#include "cmd_completion.h"
#include "cmd_core.h"
#include "dir_stack.h"
#include "event_loop.h"
#include "filelist.h"
#include "filetype.h"
#include "flist_hist.h"
#include "flist_pos.h"
#include "fops_common.h"
#include "ipc.h"
#include "marks.h"
#include "ops.h"
#include "opt_handlers.h"
#include "registers.h"
#include "running.h"
#include "signals.h"
#include "status.h"
#include "trash.h"
#include "undo.h"

static int vifm_main(int argc, char *argv[]);
static int get_start_cwd(char buf[], size_t buf_len);
static int undo_perform_func(OPS op, void *data, const char src[],
		const char dst[]);
static void parse_received_arguments(char *args[]);
static char * eval_received_expression(const char expr[]);
static void remote_cd(view_t *view, const char path[], int handle);
static void check_path_for_file(view_t *view, const char path[], int handle);
static int need_to_switch_active_pane(const char lwin_path[],
		const char rwin_path[]);
static void load_scheme(void);
static void exec_startup_commands(const args_t *args);
static void _gnuc_noreturn vifm_leave(int exit_code, int cquit);

/* Command-line arguments in parsed form. */
static args_t vifm_args;

#ifndef _WIN32

int
main(int argc, char *argv[])
{
	vifm_exit(vifm_main(argc, argv));
}

#else

int
main()
{
	extern int _CRT_glob;
	extern void __wgetmainargs(int *, wchar_t ***, wchar_t ***, int, int *);

	char **utf8_argv;
	int i;

	wchar_t **envp, **argv;
	int argc, si = 0;
	__wgetmainargs(&argc, &argv, &envp, _CRT_glob, &si);

	utf8_argv = reallocarray(NULL, argc + 1, sizeof(*utf8_argv));

	for(i = 0; i < argc; ++i)
	{
		utf8_argv[i] = utf8_from_utf16(argv[i]);
	}
	utf8_argv[i] = NULL;

	vifm_exit(vifm_main(argc, utf8_argv));
}

#endif

/* Entry-point.  Has same semantics as main(). */
static int
vifm_main(int argc, char *argv[])
{
	/* TODO: refactor vifm_main() function */

	static const int quit = 0;

	char **files = NULL;
	int nfiles = 0;
	int lwin_cv, rwin_cv;

	char dir[PATH_MAX + 1];
	if(get_start_cwd(dir, sizeof(dir)) != 0)
	{
		return -1;
	}

	args_parse(&vifm_args, argc, argv, dir);
	args_process(&vifm_args, 1);

	lwin_cv = (strcmp(vifm_args.lwin_path, "-") == 0 && vifm_args.lwin_handle);
	rwin_cv = (strcmp(vifm_args.rwin_path, "-") == 0 && vifm_args.rwin_handle);
	if(lwin_cv || rwin_cv)
	{
		files = read_stream_lines(stdin, &nfiles, 1, NULL, NULL);
		if(reopen_term_stdin() != 0)
		{
			free_string_array(files, nfiles);
			return EXIT_FAILURE;
		}
	}

	(void)setlocale(LC_ALL, "");
	srand(time(NULL));

	cfg_init();

	if(vifm_args.logging)
	{
		init_logger(1, vifm_args.startup_log_path);
	}

	init_filelists();
	tabs_init();
	regs_init();
	cfg_discover_paths();
	reinit_logger(cfg.log_file);

	/* Commands module also initializes bracket notation and variables. */
	init_commands();

	init_builtin_functions();
	update_path_env(1);

	if(stats_init(&cfg) != 0)
	{
		free_string_array(files, nfiles);

		puts("Error during session status initialization.");
		return -1;
	}

	/* Tell file type module what function to use to check availability of
	 * external programs. */
	ft_init(&external_command_exists);
	/* This should be called before loading any configuration file. */
	ft_reset(curr_stats.exec_env_type == EET_EMULATOR_WITH_X);

	init_option_handlers();

	if(!vifm_args.no_configs)
	{
		/* vifminfo must be processed this early so that it can restore last visited
		 * directory. */
		read_info_file(0);
	}

	curr_stats.ipc = ipc_init(vifm_args.server_name, &parse_received_arguments,
			&eval_received_expression);
	if(ipc_enabled() && curr_stats.ipc == NULL)
	{
		fputs("Failed to initialize IPC unit", stderr);
		return -1;
	}
	/* Export chosen server name to parsing unit. */
	{
		var_t var = var_from_str(ipc_get_name(curr_stats.ipc));
		setvar("v:servername", var);
		var_free(var);
	}

	args_process(&vifm_args, 0);

	bg_init();

	fops_init(&enter_prompt_mode, &prompt_msg_custom);

	set_view_path(&lwin, vifm_args.lwin_path);
	set_view_path(&rwin, vifm_args.rwin_path);

	if(need_to_switch_active_pane(vifm_args.lwin_path, vifm_args.rwin_path))
	{
		swap_view_roles();
	}

	load_initial_directory(&lwin, dir);
	load_initial_directory(&rwin, dir);

	/* Force split view when two paths are specified on command-line. */
	if(vifm_args.lwin_path[0] != '\0' && vifm_args.rwin_path[0] != '\0')
	{
		curr_stats.number_of_windows = 2;
	}

	/* Prepare terminal for further operations. */
	curr_stats.original_stdout = reopen_term_stdout();
	if(curr_stats.original_stdout == NULL)
	{
		free_string_array(files, nfiles);
		return -1;
	}

	if(!setup_ncurses_interface())
	{
		free_string_array(files, nfiles);
		return -1;
	}

	init_modes();
	un_init(&undo_perform_func, NULL, &ui_cancellation_requested,
			&cfg.undo_levels);
	load_view_options(curr_view);

	curr_stats.load_stage = 1;

	/* Make v:count exist during processing configuration. */
	set_count_vars(0);

	if(!vifm_args.no_configs)
	{
		load_scheme();
		cfg_load();
	}

	if(lwin_cv || rwin_cv)
	{
		flist_custom_set(lwin_cv ? &lwin : &rwin, "-", dir, files, nfiles);
	}
	free_string_array(files, nfiles);

	cs_load_pairs();
	cs_write();
	setup_signals();

	/* Ensure trash directories exist, it might not have been called during
	 * configuration file sourcing if there is no `set trashdir=...` command. */
	(void)set_trash_dir(cfg.trash_dir);

	check_path_for_file(&lwin, vifm_args.lwin_path, vifm_args.lwin_handle);
	check_path_for_file(&rwin, vifm_args.rwin_path, vifm_args.rwin_handle);

	curr_stats.load_stage = 2;

	/* Update histories of the views to ensure that their current directories,
	 * which might have been set using command-line parameters, are stored in the
	 * history.  This is not done automatically as history manipulation should be
	 * postponed until views are fully loaded, otherwise there is no correct
	 * information about current file and relative cursor position. */
	flist_hist_save(&lwin, NULL, NULL, -1);
	flist_hist_save(&rwin, NULL, NULL, -1);

	/* Trigger auto-commands for initial directories. */
	if(!lwin_cv)
	{
		(void)vifm_chdir(flist_get_dir(&lwin));
		vle_aucmd_execute("DirEnter", flist_get_dir(&lwin), &lwin);
	}
	if(!rwin_cv)
	{
		(void)vifm_chdir(flist_get_dir(&rwin));
		vle_aucmd_execute("DirEnter", flist_get_dir(&rwin), &rwin);
	}

	update_screen(UT_FULL);
	modes_update();

	/* Run startup commands after loading file lists into views, so that commands
	 * like +1 work. */
	exec_startup_commands(&vifm_args);

	curr_stats.load_stage = 3;

	event_loop(&quit);

	return 0;
}

/* Loads original working directory of the process attempting to avoid resolving
 * symbolic links in the path.  Returns zero on success, otherwise non-zero is
 * returned. */
static int
get_start_cwd(char buf[], size_t buf_len)
{
	if(get_cwd(buf, buf_len) == NULL)
	{
		perror("getcwd");
		return -1;
	}

	/* If $PWD points to the same location as CWD, use its value to preserve
	 * symbolic links in the path. */
	const char *pwd = env_get("PWD");
	if(pwd != NULL && paths_are_same(pwd, buf))
	{
		copy_str(buf, buf_len, pwd);
	}
	return 0;
}

/* perform_operation() interface adaptor for the undo unit. */
static int
undo_perform_func(OPS op, void *data, const char src[], const char dst[])
{
	return perform_operation(op, NULL, data, src, dst);
}

/* Handles arguments received from remote instance. */
static void
parse_received_arguments(char *argv[])
{
	args_t args = {};

	(void)vifm_chdir(argv[0]);
	opterr = 0;
	args_parse(&args, count_strings(argv), argv, argv[0]);
	args_process(&args, 0);

	exec_startup_commands(&args);
	args_free(&args);

	if(NONE(vle_mode_is, NORMAL_MODE, VIEW_MODE))
	{
		return;
	}

#ifdef _WIN32
	SwitchToThisWindow(GetConsoleWindow(), TRUE);
	BringWindowToTop(GetConsoleWindow());
	SetForegroundWindow(GetConsoleWindow());
#endif

	if(view_needs_cd(&lwin, args.lwin_path))
	{
		remote_cd(&lwin, args.lwin_path, args.lwin_handle);
	}

	if(view_needs_cd(&rwin, args.rwin_path))
	{
		remote_cd(&rwin, args.rwin_path, args.rwin_handle);
	}

	if(need_to_switch_active_pane(args.lwin_path, args.rwin_path))
	{
		change_window();
	}

	/* XXX: why force clearing of the statusbar? */
	ui_sb_clear();
	curr_stats.save_msg = 0;
}

static void
remote_cd(view_t *view, const char path[], int handle)
{
	char buf[PATH_MAX + 1];

	if(view->explore_mode)
	{
		view_leave_mode();
	}

	if(view == other_view && vle_mode_is(VIEW_MODE))
	{
		view_leave_mode();
	}

	if(curr_stats.preview.on && (handle || view == other_view))
	{
		qv_toggle();
	}

	copy_str(buf, sizeof(buf), path);
	exclude_file_name(buf);

	(void)cd(view, view->curr_dir, buf);
	check_path_for_file(view, path, handle);
}

/* Navigates to/opens (handles) file specified by the path (and file only, no
 * directories). */
static void
check_path_for_file(view_t *view, const char path[], int handle)
{
	if(path[0] == '\0' || is_dir(path) || (handle && strcmp(path, "-") == 0))
	{
		return;
	}

	load_dir_list(view, 1);
	if(fpos_ensure_selected(view, after_last(path, '/')))
	{
		if(handle)
		{
			open_file(view, FHE_RUN);
		}
	}
}

/* Decides whether active view should be switched based on paths provided for
 * panes on the command-line.  Returns non-zero if so, otherwise zero is
 * returned. */
static int
need_to_switch_active_pane(const char lwin_path[], const char rwin_path[])
{
	/* Forces view switch when path is specified for invisible pane. */
	return lwin_path[0] != '\0'
	    && rwin_path[0] == '\0'
	    && curr_view != &lwin;
}

/* Evaluates expression from a remote instance.  Returns newly allocated string
 * with the result or NULL on error. */
static char *
eval_received_expression(const char expr[])
{
	char *result_str;

	var_t result;
	if(parse(expr, 1, &result) != PE_NO_ERROR)
	{
		return NULL;
	}

	result_str = var_to_str(result);
	var_free(result);
	return result_str;
}

/* Loads color scheme.  Converts old format to the new one if needed. */
static void
load_scheme(void)
{
	if(cs_have_no_extensions())
	{
		cs_rename_all();
	}

	if(cs_exists(curr_stats.color_scheme))
	{
		cs_load_primary(curr_stats.color_scheme);
	}
}

void
vifm_restart(void)
{
	view_t *tmp_view;

	curr_stats.restart_in_progress = 1;

	/* All user mappings in all modes. */
	vle_keys_user_clear();

	/* User defined commands. */
	vle_cmds_run("comclear");

	/* Autocommands. */
	vle_aucmd_remove(NULL, NULL);

	/* All kinds of histories. */
	cfg_resize_histories(0);

	/* Session status.  Must be reset _before_ options, because options take some
	 * of values from status. */
	(void)stats_reset(&cfg);

	/* Options of current pane. */
	vle_opts_restore_defaults();
	/* Options of other pane. */
	tmp_view = curr_view;
	curr_view = other_view;
	load_view_options(other_view);
	vle_opts_restore_defaults();
	curr_view = tmp_view;

	/* File types and viewers. */
	ft_reset(curr_stats.exec_env_type == EET_EMULATOR_WITH_X);

	/* Undo list. */
	un_reset();

	/* Directory stack. */
	dir_stack_clear();

	/* Registers. */
	regs_reset();

	/* Clear all marks and bookmarks. */
	clear_all_marks();
	bmarks_clear();

	/* Reset variables. */
	clear_envvars();
	init_variables();
	/* This update is needed as clear_variables() will reset $PATH. */
	update_path_env(1);

	reset_views();
	read_info_file(1);
	flist_hist_save(&lwin, NULL, NULL, -1);
	flist_hist_save(&rwin, NULL, NULL, -1);

	/* Color schemes. */
	if(stroscmp(curr_stats.color_scheme, DEF_CS_NAME) != 0 &&
			cs_exists(curr_stats.color_scheme))
	{
		cs_load_primary(curr_stats.color_scheme);
	}
	else
	{
		cs_load_defaults();
	}
	cs_load_pairs();

	cfg_load();

	/* Reloading of tabs needs to happen after configuration is read so that new
	 * values from lwin and rwin got propagated. */
	tabs_reload();

	exec_startup_commands(&vifm_args);

	curr_stats.restart_in_progress = 0;

	/* Trigger auto-commands for initial directories. */
	(void)vifm_chdir(flist_get_dir(&lwin));
	vle_aucmd_execute("DirEnter", flist_get_dir(&lwin), &lwin);
	(void)vifm_chdir(flist_get_dir(&rwin));
	vle_aucmd_execute("DirEnter", flist_get_dir(&rwin), &rwin);

	update_screen(UT_REDRAW);
}

/* Executes list of startup commands. */
static void
exec_startup_commands(const args_t *args)
{
	size_t i;
	for(i = 0; i < args->ncmds; ++i)
	{
		/* Make sure we're executing commands in correct directory. */
		(void)vifm_chdir(flist_get_dir(curr_view));

		(void)exec_commands(args->cmds[i], curr_view, CIT_COMMAND);
	}
}

void
vifm_try_leave(int write_info, int cquit, int force)
{
	if(!force && bg_has_active_jobs())
	{
		if(!prompt_msg("Warning", "Some of backgrounded commands are still "
					"working.  Quit?"))
		{
			return;
		}
	}

	fuse_unmount_all();

	if(write_info)
	{
		write_info_file();
	}

	if(stats_file_choose_action_set())
	{
		vim_write_empty_file_list();
	}
#ifdef _WIN32
	erase();
	refresh();
#endif
	ui_shutdown();

	vifm_leave(EXIT_SUCCESS, cquit);
}

void _gnuc_noreturn
vifm_choose_files(const view_t *view, int nfiles, char *files[])
{
	int exit_code;

	/* As curses can do something with terminal on shutting down, disable it
	 * before writing anything to the screen. */
	ui_shutdown();

	exit_code = EXIT_SUCCESS;
	if(vim_write_file_list(view, nfiles, files) != 0)
	{
		exit_code = EXIT_FAILURE;
	}
	/* XXX: this ignores nfiles+files. */
	if(vim_run_choose_cmd(view) != 0)
	{
		exit_code = EXIT_FAILURE;
	}

	write_info_file();

	vifm_leave(exit_code, 0);
}

/* Single exit point for leaving vifm, performs only minimum common
 * deinitialization steps. */
static void _gnuc_noreturn
vifm_leave(int exit_code, int cquit)
{
	vim_write_dir(cquit ? "" : flist_get_dir(curr_view));

	if(cquit && exit_code == EXIT_SUCCESS)
	{
		exit_code = EXIT_FAILURE;
	}

	term_title_update(NULL);
	vifm_exit(exit_code);
}

void _gnuc_noreturn
vifm_finish(const char message[])
{
	ui_shutdown();

	/* Update vifminfo only if we were able to startup, otherwise we can end up
	 * writing from some intermediate half-initialized state.  One particular
	 * case: after vifminfo read, but before configuration is processed, as a
	 * result we write very little information to vifminfo file according to
	 * default value of 'vifminfo' option. */
	if(curr_stats.load_stage == 3)
	{
		write_info_file();
	}

	fprintf(stderr, "%s\n", message);
	LOG_ERROR_MSG("Finishing: %s", message);
	vifm_exit(EXIT_FAILURE);
}

void _gnuc_noreturn
vifm_exit(int exit_code)
{
	ipc_free(curr_stats.ipc);
	exit(exit_code);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
