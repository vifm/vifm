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

#include "../config.h"

#ifdef _WIN32
#define _WIN32_WINNT 0x0500
#include <tchar.h>
#include <windows.h>
#endif

#ifdef HAVE_LIBGTK
#include <glib-2.0/gio/gio.h>
#include <gtk/gtk.h>
#endif

#include <curses.h>

#include <dirent.h> /* DIR */
#include <unistd.h> /* getcwd, stat, sysconf */

#include <locale.h> /* setlocale */
#include <string.h> /* strncpy */

#include "background.h"
#include "bookmarks.h"
#include "color_scheme.h"
#include "commands.h"
#include "config.h"
#include "filelist.h"
#include "fileops.h"
#include "filetype.h"
#include "ipc.h"
#include "log.h"
#include "macros.h"
#include "main_loop.h"
#include "menus.h"
#include "modes.h"
#include "normal.h"
#include "ops.h"
#include "opt_handlers.h"
#include "registers.h"
#include "signals.h"
#include "sort.h"
#include "status.h"
#include "tree.h"
#include "ui.h"
#include "undo.h"
#include "utils.h"
#include "variables.h"
#include "view.h"

#ifndef _WIN32
#define CONF_DIR "~/.vifm"
#else
#define CONF_DIR "(%HOME%/.vifm or %APPDATA%/Vifm)"
#endif

static void quit_on_invalid_arg(void);
static void parse_recieved_arguments(char *args[]);
static void remote_cd(FileView *view, const char *path, int handle);

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
		free(list[i]);
	}

	free(list);
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
	puts("  vifm --remote");
	puts("    passes all arguments that left in command line to active vifm server.\n");
	puts("  vifm -c <command> | +<command>");
	puts("    run <command> on startup.\n");
	puts("  vifm --version | -v");
	puts("    show version number and quit.\n");
	puts("  vifm --help | -h");
	puts("    show this help message and quit.\n");
	puts("  vifm --no-configs");
	puts("    don't read vifmrc and vifminfo.");
}

static void
init_window(FileView *win)
{
	win->curr_line = 0;
	win->top_line = 0;
	win->list_rows = 0;
	win->list_pos = 0;
	win->selected_filelist = NULL;
	win->history_num = 0;
	win->history_pos = 0;
	win->invert = 0;
	win->color_scheme = 1;
}

static void
init_window_history(FileView *win)
{
	if(cfg.history_len == 0)
		return;

	win->history = malloc(sizeof(history_t)*cfg.history_len);
	while(win->history == NULL)
	{
		cfg.history_len /= 2;
		win->history = malloc(sizeof(history_t)*cfg.history_len);
	}
}

static void
load_initial_directory(FileView *view, const char *dir)
{
	if(view->curr_dir[0] == '\0')
		snprintf(view->curr_dir, sizeof(view->curr_dir), "%s", dir);
	else
		dir = view->curr_dir;

	view->dir_entry = (dir_entry_t *)malloc(sizeof(dir_entry_t));
	memset(view->dir_entry, 0, sizeof(dir_entry_t));

	view->dir_entry[0].name = malloc(sizeof("../") + 1);
	strcpy(view->dir_entry[0].name, "../");
	view->dir_entry[0].type = DIRECTORY;

	view->list_rows = 1;
	chosp(view->curr_dir);
	(void)change_directory(view, dir);
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

	(void)my_chdir(dir);

	/* Get Command Line Arguments */
	for(x = 1; x < argc; x++)
	{
		if(!strcmp(argv[x], "--select"))
		{
			select = 1;
		}
		else if(!strcmp(argv[x], "--remote"))
		{
			if(!ipc_server())
			{
				ipc_send(argv + x + 1);
				quit_on_invalid_arg();
			}
		}
		else if(!strcmp(argv[x], "-f"))
		{
			cfg.vim_filter = 1;
		}
		else if(!strcmp(argv[x], "--no-configs"))
		{
		}
		else if(!strcmp(argv[x], "--version") || !strcmp(argv[x], "-v"))
		{
			show_version_msg();
			quit_on_invalid_arg();
		}
		else if(!strcmp(argv[x], "--help") || !strcmp(argv[x], "-h"))
		{
			show_help_msg();
			quit_on_invalid_arg();
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
				quit_on_invalid_arg();
			}
			/* do nothing, it's handeled in exec_startup_commands() */
			x++;
		}
		else if(argv[x][0] == '+')
		{
			/* do nothing, it's handeled in exec_startup_commands() */
		}
		else if(access(argv[x], F_OK) == 0 || is_path_absolute(argv[x]) ||
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
			quit_on_invalid_arg();
		}
		else
		{
			(void)show_error_msgf("--remote error", "Invalid argument: %s", argv[x]);
		}
	}
}

static void
quit_on_invalid_arg(void)
{
	if(curr_stats.load_stage == 0)
		exit(1);
}

static void
add_to_path(const char *str)
{
	const char *old_path;
	char *new_path;

	old_path = env_get("PATH");
	new_path = malloc(5 + strlen(str) + 1 + strlen(old_path) + 1);

#ifndef _WIN32
	sprintf(new_path, "%s:%s", str, old_path);
#else
	sprintf(new_path, "%s;%s", str, old_path);
	to_back_slash(new_path);
#endif
	env_set("PATH", new_path);

	free(new_path);
}

/* Returns non-zero if path should be changed. */
static int
check_path(FileView *view, const char *path)
{
	if(path[0] == '\0' || pathcmp(view->curr_dir, path) == 0)
		return 0;
	return 1;
}

/* Removes file name from path. */
static void
exclude_file_name(char *path)
{
	if(file_exists(NULL, path) && !is_dir(path) && !is_unc_root(path))
		break_atr(path, '/');
}

/* Sets view's current directory from path value.
 * Returns non-zero if view's directory was changed. */
static int
set_path(FileView *view, const char *path)
{
	if(!check_path(view, path))
		return 0;

	strcpy(view->curr_dir, path);
	exclude_file_name(view->curr_dir);
	return 1;
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

static int
run_converter(int vifm_like)
{
#ifndef _WIN32
	char buf[PATH_MAX];
	snprintf(buf, sizeof(buf), "vifmrc-converter %d", vifm_like);
	return shellout(buf, -1, 1);
#else
	TCHAR buf[PATH_MAX + 2];

	if(GetModuleFileName(NULL, buf, ARRAY_LEN(buf)) == 0)
		return -1;

	*(_tcsrchr(buf, _T('\\')) + 1) = _T('\0');
	if(vifm_like == 2)
		_tcscat(buf, _T("vifmrc-converter 2"));
	else if(vifm_like == 1)
		_tcscat(buf, _T("vifmrc-converter 1"));
	else
		_tcscat(buf, _T("vifmrc-converter 0"));

	return exec_program(buf);
#endif
}

void
add_dirs_to_path(const char *path)
{
	DIR* dir;
	struct dirent* dentry;
	const char* slash = "";

	dir = opendir(path);
	if(dir == NULL)
		return;

	if(path[strlen(path) - 1] != '/')
		slash = "/";

	add_to_path(path);

	while((dentry = readdir(dir)) != NULL)
	{
		char buf[PATH_MAX];

		if(pathcmp(dentry->d_name, ".") == 0)
			continue;
		else if(pathcmp(dentry->d_name, "..") == 0)
			continue;

		snprintf(buf, sizeof(buf), "%s%s%s", path, slash, dentry->d_name);
#ifndef _WIN32
		if(dentry->d_type == DT_DIR)
#else
		if(is_dir(buf))
#endif
		{
			add_dirs_to_path(buf);
		}
	}

	closedir(dir);
}

int
main(int argc, char *argv[])
{
	/* TODO: refactor main() function */

	char dir[PATH_MAX];
	char config_dir[PATH_MAX];
	const char *console;
	char lwin_path[PATH_MAX] = "";
	char rwin_path[PATH_MAX] = "";
	int lwin_handle = 0, rwin_handle = 0;
	int old_config;
	int i;
	int no_configs;
	int lcd, rcd;

	init_config();

	for(i = 1; i < argc; i++)
		if(strcmp(argv[i], "--logging") == 0)
			init_logger(1);

	(void)setlocale(LC_ALL, "");
	if(getcwd(dir, sizeof(dir)) == NULL)
	{
		perror("getcwd");
		return -1;
	}
#ifdef _WIN32
	to_forward_slash(dir);
#endif

	init_window(&rwin);
	init_window(&lwin);

	filetypes = NULL;

	init_registers();
	set_config_paths();
	reinit_logger();

	snprintf(config_dir, sizeof(config_dir), "%s/scripts", cfg.config_dir);
	add_dirs_to_path(config_dir);

	init_commands();
	load_default_configuration();

	/* Safety check for existing vifmrc file without FUSE_HOME */
	if(cfg.fuse_home == NULL)
		cfg.fuse_home = strdup("/tmp/vifm_FUSE");

	/* Misc configuration */

	lwin.prev_invert = lwin.invert;
	lwin.hide_dot = 1;
	strncpy(lwin.regexp, "", sizeof(lwin.regexp));
	lwin.matches = 0;
	init_window_history(&lwin);

	rwin.prev_invert = rwin.invert;
	rwin.hide_dot = 1;
	strncpy(rwin.regexp, "", sizeof(rwin.regexp));
	rwin.matches = 0;
	init_window_history(&rwin);

	init_status();
	curr_stats.dirsize_cache = tree_create(0, 0);
	if(curr_stats.dirsize_cache == NULL)
	{
		puts("Not enough memory for initialization");
		return -1;
	}

#ifdef HAVE_LIBGTK
	curr_stats.gtk_available = gtk_init_check(&argc, &argv);
#endif

	if(cfg.show_one_window)
		curr_stats.number_of_windows = 1;
	else
		curr_stats.number_of_windows = 2;

	snprintf(config_dir, sizeof(config_dir), "%s/vifmrc", cfg.config_dir);

	/* Check if running in X */
#ifndef _WIN32
	console = env_get("DISPLAY");
#else
	console = "WIN";
#endif
	if(!console || !*console)
		curr_stats.is_console = 1;

	curr_view = &lwin;
	other_view = &rwin;

	no_configs = 0;
	for(i = 1; i < argc; i++)
		if(strcmp("--no-configs", argv[i]) == 0)
			no_configs = 1;

	old_config = is_old_config();
	if(!old_config && !no_configs)
		read_info_file(0);

	ipc_init(&parse_recieved_arguments);

	parse_args(argc, argv, dir, lwin_path, rwin_path, &lwin_handle, &rwin_handle);

	lcd = set_path(&lwin, lwin_path);
	rcd = set_path(&rwin, rwin_path);

	if(lcd && !rcd && curr_view != &lwin)
		change_window();

	load_initial_directory(&lwin, dir);
	load_initial_directory(&rwin, dir);

	/* Setup the ncurses interface. */
	if(!setup_ncurses_interface())
		return -1;

	init_modes();
	init_option_handlers();
	init_undo_list(&perform_operation, &cfg.undo_levels);
	load_local_options(curr_view);

	curr_stats.load_stage = 1;

	if(!old_config && !no_configs)
	{
		if(are_old_color_schemes())
		{
			if(run_converter(2) != 0)
			{
				endwin();
				fputs("Problems with running vifmrc-converter", stderr);
				exit(0);
			}
		}
		if(find_color_scheme(curr_stats.color_scheme))
			load_color_scheme(curr_stats.color_scheme);
		load_color_scheme_colors();
		exec_config();
	}

	write_color_scheme_file();
	setup_signals();

	if(old_config && !no_configs)
	{
		int vifm_like;
		int result;
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

		vifm_like = !query_user_menu("Configuration update", "This version of vifm "
				"is able to save changes in the configuration files automatically when "
				"quitting, as it was possible in older versions.  It is from now on "
				"recommended though, to save permanent changes manually in the "
				"configuration file as it is done in vi/vim.  Do you want vifm to "
				"behave like vi/vim?");

		result = run_converter(vifm_like);
		if(result != 0)
		{
			endwin();
			fputs("Problems with running vifmrc-converter", stderr);
			exit(0);
		}

		(void)show_error_msg("Configuration update", "Your vifmrc has been "
				"upgraded to "
				"new format, you can find its old version in " CONF_DIR "/vifmrc.bak.  "
				"vifm will not write anything to vifmrc, and all variables that are "
				"saved between runs of vifm are stored in " CONF_DIR "/vifminfo now "
				"(you can edit it by hand, but do it carefully).  You can control what "
				"vifm stores in vifminfo with 'vifminfo' option.");

		curr_stats.load_stage = 0;
		read_info_file(0);
		curr_stats.load_stage = 1;

		set_path(&lwin, lwin_path);
		set_path(&rwin, rwin_path);

		load_initial_directory(&lwin, dir);
		load_initial_directory(&rwin, dir);

		exec_config();
	}

	create_trash_dir();

	check_path_for_file(&lwin, lwin_path, lwin_handle);
	check_path_for_file(&rwin, rwin_path, rwin_handle);

	curr_stats.load_stage = 2;

	exec_startup_commands(argc, argv);

	modes_redraw();

	main_loop();

	return 0;
}

static void
parse_recieved_arguments(char *args[])
{
	char lwin_path[PATH_MAX] = "";
	char rwin_path[PATH_MAX] = "";
	int lwin_handle = 0, rwin_handle = 0;
	int argc = 0;
	int lcd, rcd;

	while(args[argc] != NULL)
		argc++;

	parse_args(argc, args, args[0], lwin_path, rwin_path, &lwin_handle,
			&rwin_handle);
	exec_startup_commands(argc, args);

	if(get_mode() != NORMAL_MODE && get_mode() != VIEW_MODE)
		return;

#ifdef _WIN32
	SwitchToThisWindow(GetConsoleWindow(), TRUE);
	BringWindowToTop(GetConsoleWindow());
	SetForegroundWindow(GetConsoleWindow());
#endif

	if((lcd = check_path(&lwin, lwin_path)))
		remote_cd(&lwin, lwin_path, lwin_handle);

	if((rcd = check_path(&rwin, rwin_path)))
		remote_cd(&rwin, rwin_path, rwin_handle);

	if(lcd && !rcd && curr_view != &lwin)
		change_window();

	clean_status_bar();
	curr_stats.save_msg = 0;
}

static void
remote_cd(FileView *view, const char *path, int handle)
{
	char buf[PATH_MAX];

	if(view->explore_mode)
		leave_view_mode();

	if(view == other_view && get_mode() == VIEW_MODE)
		leave_view_mode();

	if(curr_stats.view)
		toggle_quick_view();

	snprintf(buf, sizeof(buf), "%s", path);
	exclude_file_name(buf);

	(void)cd(view, buf);
	check_path_for_file(view, path, handle);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
