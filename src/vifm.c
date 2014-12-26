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
#include <tchar.h>
#include <windows.h>
#endif

#include <curses.h>

#include <unistd.h> /* getcwd, stat, sysconf */

#include <errno.h> /* errno */
#include <locale.h> /* setlocale */
#include <stddef.h> /* NULL */
#include <stdio.h> /* fprintf() fputs() puts() snprintf() */
#include <stdlib.h> /* EXIT_FAILURE EXIT_SUCCESS exit() system() */
#include <string.h>

#include "cfg/config.h"
#include "cfg/info.h"
#include "engine/cmds.h"
#include "engine/keys.h"
#include "engine/mode.h"
#include "engine/options.h"
#include "engine/variables.h"
#include "menus/menus.h"
#include "modes/modes.h"
#include "modes/view.h"
#include "ui/cancellation.h"
#include "ui/statusbar.h"
#include "ui/ui.h"
#include "utils/fs.h"
#include "utils/fs_limits.h"
#include "utils/log.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/utils.h"
#include "background.h"
#include "bookmarks.h"
#include "bracket_notation.h"
#include "builtin_functions.h"
#include "color_manager.h"
#include "color_scheme.h"
#include "commands.h"
#include "commands_completion.h"
#include "dir_stack.h"
#include "filelist.h"
#include "fileops.h"
#include "filetype.h"
#include "fuse.h"
#include "ipc.h"
#include "main_loop.h"
#include "ops.h"
#include "opt_handlers.h"
#include "path_env.h"
#include "quickview.h"
#include "registers.h"
#include "running.h"
#include "signals.h"
#include "status.h"
#include "term_title.h"
#include "trash.h"
#include "undo.h"
#include "version.h"
#include "vim.h"

#ifndef _WIN32
#define CONF_DIR "~/.vifm"
#else
#define CONF_DIR "(%HOME%/.vifm or %APPDATA%/Vifm)"
#endif

static void quit_on_arg_parsing(void);
static int undo_perform_func(OPS op, void *data, const char src[],
		const char dst[]);
static void parse_recieved_arguments(char *args[]);
static void remote_cd(FileView *view, const char *path, int handle);
static int need_to_switch_active_pane(const char lwin_path[],
		const char rwin_path[]);
static void load_scheme(void);
static void convert_configs(void);
static int run_converter(int vifm_like_mode);

static void
show_version_msg(void)
{
	int i, len;
	char **list;
	list = malloc(sizeof(char*)*fill_version_info(NULL));

	len = fill_version_info(list);
	for(i = 0; i < len; i++)
	{
		puts(list[i]);
	}

	free_string_array(list, len);
}

static void
show_help_msg(void)
{
	puts("vifm usage:\n");
	puts("  To start in a specific directory give the directory path.\n");
	puts("    vifm /path/to/start/dir/one");
	puts("    or");
	puts("    vifm /path/to/start/dir/one  /path/to/start/dir/two\n");
	puts("  To open file using associated program pass to vifm it's path.\n");
	puts("  To select file prepend its path with --select.\n");
	puts("  If no path is given vifm will start in the current working directory.\n");
	puts("  vifm --logging");
	puts("    log some errors to " CONF_DIR "/log.\n");
#ifdef ENABLE_REMOTE_CMDS
	puts("  vifm --remote");
	puts("    passes all arguments that left in command line to active vifm server.\n");
#endif
	puts("  vifm -c <command> | +<command>");
	puts("    run <command> on startup.\n");
	puts("  vifm --version | -v");
	puts("    show version number and quit.\n");
	puts("  vifm --help | -h");
	puts("    show this help message and quit.\n");
	puts("  vifm --no-configs");
	puts("    don't read vifmrc and vifminfo.");
}

/* buf should be at least PATH_MAX characters length */
static void
parse_path(const char *dir, const char *path, char *buf)
{
	strcpy(buf, path);
#ifdef _WIN32
	to_forward_slash(buf);
#endif
	if(is_path_absolute(buf))
	{
		snprintf(buf, PATH_MAX, "%s", path);
	}
#ifdef _WIN32
	else if(buf[0] == '/')
	{
		snprintf(buf, PATH_MAX, "%c:%s", dir[0], path);
	}
#endif
	else
	{
		char new_path[PATH_MAX];
		snprintf(new_path, sizeof(new_path), "%s/%s", dir, path);
		canonicalize_path(new_path, buf, PATH_MAX);
	}
	if(!is_root_dir(buf))
		chosp(buf);

#ifdef _WIN32
	to_forward_slash(buf);
#endif
}

static void
parse_args(int argc, char *argv[], const char *dir, char *lwin_path,
		char *rwin_path, int *lwin_handle, int *rwin_handle)
{
	int x;
	int select = 0;

	(void)vifm_chdir(dir);

	/* Get Command Line Arguments */
	for(x = 1; x < argc; x++)
	{
		if(!strcmp(argv[x], "--select"))
		{
			select = 1;
		}
#ifdef ENABLE_REMOTE_CMDS
		else if(!strcmp(argv[x], "--remote"))
		{
			if(!ipc_server())
			{
				ipc_send(argv + x + 1);
				quit_on_arg_parsing();
			}
		}
#endif
		else if(!strcmp(argv[x], "-f"))
		{
			curr_stats.file_picker_mode = 1;
		}
		else if(!strcmp(argv[x], "--no-configs"))
		{
			/* Do nothing. */
		}
		else if(!strcmp(argv[x], "--version") || !strcmp(argv[x], "-v"))
		{
			show_version_msg();
			quit_on_arg_parsing();
		}
		else if(!strcmp(argv[x], "--help") || !strcmp(argv[x], "-h"))
		{
			show_help_msg();
			quit_on_arg_parsing();
		}
		else if(!strcmp(argv[x], "--logging"))
		{
			/* do nothing, it's handeled in main() */
		}
		else if(!strcmp(argv[x], "-c"))
		{
			if(x == argc - 1)
			{
				puts("Argument missing after \"-c\"");
				quit_on_arg_parsing();
			}
			/* do nothing, it's handeled in exec_startup_commands() */
			x++;
		}
		else if(argv[x][0] == '+')
		{
			/* do nothing, it's handeled in exec_startup_commands() */
		}
		else if(path_exists(argv[x], DEREF) || is_path_absolute(argv[x]) ||
				is_root_dir(argv[x]))
		{
			if(lwin_path[0] != '\0')
			{
				parse_path(dir, argv[x], rwin_path);
				*rwin_handle = !select;
			}
			else
			{
				parse_path(dir, argv[x], lwin_path);
				*lwin_handle = !select;
			}
			select = 0;
		}
		else if(curr_stats.load_stage == 0)
		{
			show_help_msg();
			quit_on_arg_parsing();
		}
#ifdef ENABLE_REMOTE_CMDS
		else
		{
			show_error_msgf("--remote error", "Invalid argument: %s", argv[x]);
		}
#endif
	}
}

/* Quits during argument parsing when it's allowed (e.g. not for remote
 * commands). */
static void
quit_on_arg_parsing(void)
{
	if(curr_stats.load_stage == 0)
	{
		exit(1);
	}
}

static void
check_path_for_file(FileView *view, const char *path, int handle)
{
	if(path[0] != '\0' && !is_dir(path))
	{
		const char *slash = strrchr(path, '/');
		if(slash == NULL)
			slash = path - 1;
		load_dir_list(view, !(cfg.vifm_info&VIFMINFO_SAVEDIRS));
		if(ensure_file_is_selected(view, slash + 1))
		{
			if(handle)
				handle_file(view, 0, 0);
		}
	}
}

int
main(int argc, char *argv[])
{
	/* TODO: refactor main() function */

	char dir[PATH_MAX];
	char lwin_path[PATH_MAX] = "";
	char rwin_path[PATH_MAX] = "";
	int lwin_handle = 0, rwin_handle = 0;
	int old_config;
	int no_configs;

	init_config();

	if(is_in_string_array(argv + 1, argc - 1, "--logging"))
	{
		init_logger(1);
	}

	(void)setlocale(LC_ALL, "");
	if(getcwd(dir, sizeof(dir)) == NULL)
	{
		perror("getcwd");
		return -1;
	}
#ifdef _WIN32
	to_forward_slash(dir);
#endif

	init_filelists();
	init_registers();
	set_config_paths();
	reinit_logger();

	/* Commands module also initializes bracket notation and variables. */
	init_commands();

	init_builtin_functions();
	update_path_env(1);

	if(init_status(&cfg) != 0)
	{
		puts("Error during session status initialization.");
		return -1;
	}

	no_configs = is_in_string_array(argv + 1, argc - 1, "--no-configs");

	/* Tell file type module what function to use to check availability of
	 * external programs. */
	ft_init(&external_command_exists);
	/* This should be called before loading any configuration file. */
	ft_reset(curr_stats.exec_env_type == EET_EMULATOR_WITH_X);

	init_option_handlers();

	old_config = is_old_config();
	if(!old_config && !no_configs)
		read_info_file(0);

	ipc_pre_init();

	parse_args(argc, argv, dir, lwin_path, rwin_path, &lwin_handle, &rwin_handle);

	ipc_init(&parse_recieved_arguments);

	init_background();

	init_fileops();

	set_view_path(&lwin, lwin_path);
	set_view_path(&rwin, rwin_path);

	if(need_to_switch_active_pane(lwin_path, rwin_path))
	{
		swap_view_roles();
	}

	load_initial_directory(&lwin, dir);
	load_initial_directory(&rwin, dir);

	/* Force split view when two paths are specified on command-line. */
	if(lwin_path[0] != '\0' && rwin_path[0] != '\0')
	{
		curr_stats.number_of_windows = 2;
	}

	/* Setup the ncurses interface. */
	if(!setup_ncurses_interface())
		return -1;

	{
		const colmgr_conf_t colmgr_conf = {
			.max_color_pairs = COLOR_PAIRS,
			.init_pair = &init_pair,
			.pair_content = &pair_content,
		};
		colmgr_init(&colmgr_conf);
	}

	init_modes();
	init_undo_list(&undo_perform_func, NULL, &ui_cancellation_requested,
			&cfg.undo_levels);
	load_local_options(curr_view);

	curr_stats.load_stage = 1;

	if(!old_config && !no_configs)
	{
		load_scheme();
		source_config();
	}

	write_color_scheme_file();
	setup_signals();

	if(old_config && !no_configs)
	{
		convert_configs();

		curr_stats.load_stage = 0;
		read_info_file(0);
		curr_stats.load_stage = 1;

		set_view_path(&lwin, lwin_path);
		set_view_path(&rwin, rwin_path);

		load_initial_directory(&lwin, dir);
		load_initial_directory(&rwin, dir);

		source_config();
	}

	/* Ensure trash directories exist, it might not have been called during
	 * configuration file sourcing if there is no `set trashdir=...` command. */
	(void)set_trash_dir(cfg.trash_dir);

	check_path_for_file(&lwin, lwin_path, lwin_handle);
	check_path_for_file(&rwin, rwin_path, rwin_handle);

	curr_stats.load_stage = 2;

	exec_startup_commands(argc, argv);
	update_screen(UT_FULL);
	modes_update();

	/* Update histories of the views to ensure that their current directories,
	 * which might have been set using command-line parameters, are stored in the
	 * history.  This is not done automatically as history manipulation should be
	 * postponed until views are fully loaded, otherwise there is no correct
	 * information about current file and relative cursor position. */
	save_view_history(&lwin, NULL, NULL, -1);
	save_view_history(&rwin, NULL, NULL, -1);

	curr_stats.load_stage = 3;

	main_loop();

	return 0;
}

/* perform_operation() interface adaptor for the undo unit. */
static int
undo_perform_func(OPS op, void *data, const char src[], const char dst[])
{
	return perform_operation(op, NULL, data, src, dst);
}

static void
parse_recieved_arguments(char *args[])
{
	char lwin_path[PATH_MAX] = "";
	char rwin_path[PATH_MAX] = "";
	int lwin_handle = 0, rwin_handle = 0;
	int argc = 0;

	while(args[argc] != NULL)
	{
		argc++;
	}

	parse_args(argc, args, args[0], lwin_path, rwin_path, &lwin_handle,
			&rwin_handle);
	exec_startup_commands(argc, args);

	if(NONE(vle_mode_is, NORMAL_MODE, VIEW_MODE))
	{
		return;
	}

#ifdef _WIN32
	SwitchToThisWindow(GetConsoleWindow(), TRUE);
	BringWindowToTop(GetConsoleWindow());
	SetForegroundWindow(GetConsoleWindow());
#endif

	if(view_needs_cd(&lwin, lwin_path))
	{
		remote_cd(&lwin, lwin_path, lwin_handle);
	}

	if(view_needs_cd(&rwin, rwin_path))
	{
		remote_cd(&rwin, rwin_path, rwin_handle);
	}

	if(need_to_switch_active_pane(lwin_path, rwin_path))
	{
		change_window();
	}

	clean_status_bar();
	curr_stats.save_msg = 0;
}

static void
remote_cd(FileView *view, const char *path, int handle)
{
	char buf[PATH_MAX];

	if(view->explore_mode)
	{
		leave_view_mode();
	}

	if(view == other_view && vle_mode_is(VIEW_MODE))
	{
		leave_view_mode();
	}

	if(curr_stats.view)
	{
		toggle_quick_view();
	}

	snprintf(buf, sizeof(buf), "%s", path);
	exclude_file_name(buf);

	(void)cd(view, view->curr_dir, buf);
	check_path_for_file(view, path, handle);
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

/* Loads color scheme.  Converts old format to the new one if needed.
 * Terminates application with error message on error. */
static void
load_scheme(void)
{
	if(are_old_color_schemes())
	{
		const int err = run_converter(2);
		if(err != 0)
		{
			endwin();
			fputs("Problems with running vifmrc-converter\n", stderr);
			exit(err);
		}
	}
	if(color_scheme_exists(curr_stats.color_scheme))
	{
		load_primary_color_scheme(curr_stats.color_scheme);
	}
	load_color_scheme_colors();
}

/* Converts old versions of configuration files to new ones.  Terminates
 * application with error message on error or when user chooses to do not update
 * anything. */
static void
convert_configs(void)
{
	int vifm_like_mode;
	int err;

	if(!query_user_menu("Configuration update", "Your vifmrc will be "
			"upgraded to a new format.  Your current configuration will be copied "
			"before performing any changes, but if you don't want to take the risk "
			"and would like to make one more copy say No to exit vifm.  Continue?"))
	{
#ifdef _WIN32
		system("cls");
#endif
		endwin();
		exit(0);
	}

	vifm_like_mode = !query_user_menu("Configuration update", "This version of "
			"vifm is able to save changes in the configuration files automatically "
			"when quitting, as it was possible in older versions.  It is from now "
			"on recommended though, to save permanent changes manually in the "
			"configuration file as it is done in vi/vim.  Do you want vifm to "
			"behave like vi/vim?");

	err = run_converter(vifm_like_mode);
	if(err != 0)
	{
		endwin();
		fputs("Problems with running vifmrc-converter\n", stderr);
		exit(err);
	}

	show_error_msg("Configuration update", "Your vifmrc has been upgraded to "
			"new format, you can find its old version in " CONF_DIR "/vifmrc.bak.  "
			"vifm will not write anything to vifmrc, and all variables that are "
			"saved between runs of vifm are stored in " CONF_DIR "/vifminfo now "
			"(you can edit it by hand, but do it carefully).  You can control what "
			"vifm stores in vifminfo with 'vifminfo' option.");
}

/* Runs vifmrc-converter in mode specified by the vifm_like_mode argument.
 * Returns zero on success, non-zero otherwise. */
static int
run_converter(int vifm_like_mode)
{
#ifndef _WIN32
	char cmd[PATH_MAX];
	snprintf(cmd, sizeof(cmd), "vifmrc-converter %d", vifm_like_mode);
	return shellout(cmd, -1, 0);
#else
	char cmd[2*PATH_MAX];
	int returned_exit_code;
	char *name_part;

	if(GetModuleFileName(NULL, cmd, PATH_MAX) == 0)
	{
		return -1;
	}

	/* Override last path component. */
	name_part = strrchr(cmd, '\\');
	name_part = (name_part == NULL) ? cmd : (name_part + 1);
	switch(vifm_like_mode)
	{
		case 2:
			strcpy(name_part, "vifmrc-converter 2");
			break;
		case 1:
			strcpy(name_part, "vifmrc-converter 1");
			break;

		default:
			strcpy(name_part, "vifmrc-converter 0");
			break;
	}

	return win_exec_cmd(cmd, &returned_exit_code);
#endif
}

void
vifm_restart(void)
{
	FileView *tmp_view;

	curr_stats.restart_in_progress = 1;

	/* All user mappings in all modes. */
	clear_user_keys();

	/* User defined commands. */
	execute_cmd("comclear");

	/* Directory histories. */
	ui_view_clear_history(&lwin);
	ui_view_clear_history(&rwin);

	/* All kinds of history. */
	(void)hist_reset(&cfg.search_hist, cfg.history_len);
	(void)hist_reset(&cfg.cmd_hist, cfg.history_len);
	(void)hist_reset(&cfg.prompt_hist, cfg.history_len);
	(void)hist_reset(&cfg.filter_hist, cfg.history_len);
	cfg.history_len = 0;

	/* Session status.  Must be reset _before_ options, because options take some
	 * of values from status. */
	(void)reset_status(&cfg);

	/* Options of current pane. */
	reset_options_to_default();
	/* Options of other pane. */
	tmp_view = curr_view;
	curr_view = other_view;
	load_local_options(other_view);
	reset_options_to_default();
	curr_view = tmp_view;

	/* File types and viewers. */
	ft_reset(curr_stats.exec_env_type == EET_EMULATOR_WITH_X);

	/* Undo list. */
	reset_undo_list();

	/* Directory stack. */
	clean_stack();

	/* Registers. */
	clear_registers();

	/* Clear all bookmarks. */
	clear_all_bookmarks();

	/* Reset variables. */
	clear_variables();
	init_variables();
	/* This update is needed as clear_variables() will reset $PATH. */
	update_path_env(1);

	reset_views();
	read_info_file(1);
	save_view_history(&lwin, NULL, NULL, -1);
	save_view_history(&rwin, NULL, NULL, -1);

	/* Color schemes. */
	if(stroscmp(curr_stats.color_scheme, DEF_CS_NAME) != 0 &&
			color_scheme_exists(curr_stats.color_scheme))
	{
		if(load_primary_color_scheme(curr_stats.color_scheme) != 0)
		{
			load_def_scheme();
		}
	}
	else
	{
		load_def_scheme();
	}
	load_color_scheme_colors();

	source_config();
	exec_startup_commands(0, NULL);

	curr_stats.restart_in_progress = 0;

	update_screen(UT_REDRAW);
}

void
vifm_try_leave(int write_info, int force)
{
	if(!force && bg_has_active_jobs())
	{
		if(!query_user_menu("Warning", "Some of backgrounded commands are still "
					"working.  Quit?"))
		{
			return;
		}
	}

	unmount_fuse();

	if(write_info)
	{
		write_info_file();
	}

	if(curr_stats.file_picker_mode)
	{
		vim_write_empty_file_list();
	}

#ifdef _WIN32
	system("cls");
#endif

	set_term_title(NULL);
	endwin();
	exit(0);
}

void _gnuc_noreturn
vifm_finish(const char message[])
{
	endwin();
	write_info_file();
	fprintf(stderr, "%s\n", message);
	LOG_ERROR_MSG("Finishing: %s", message);
	exit(EXIT_FAILURE);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
