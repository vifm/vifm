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

#include "../config.h"

#ifdef HAVE_LIBGTK
#include <glib-2.0/gio/gio.h>
#include <gtk/gtk.h>
#endif

#include <curses.h>

#include <unistd.h> /* getcwd, stat, sysconf */

#include <locale.h> /* setlocale */
#include <string.h> /* strncpy */

#include "background.h"
#include "bookmarks.h"
#include "color_scheme.h"
#include "commands.h"
#include "config.h"
#include "crc32.h"
#include "filelist.h"
#include "filetype.h"
#include "log.h"
#include "main_loop.h"
#include "modes.h"
#include "opt_handlers.h"
#include "registers.h"
#include "signals.h"
#include "sort.h"
#include "status.h"
#include "tree.h"
#include "ui.h"
#include "undo.h"
#include "utils.h"

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
//	printf("vifm %s\n", VERSION);
}

static void
show_help_msg(void)
{
	puts("vifm usage:\n");
	puts("  To start in a specific directory give the directory path.\n");
	puts("    vifm /path/to/start/dir/one");
	puts("    or");
	puts("    vifm /path/to/start/dir/one  /path/to/start/dir/two\n");
	puts("  If no path is given vifm will start in the current working directory.\n");
	puts("  vifm --logging");
	puts("    log some errors to ~/.vifm/log.\n");
	puts("  vifm --version | -v");
	puts("    show version number and quit.\n");
	puts("  vifm --help | -h");
	puts("    show this help message and quit.");
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
	win->color_scheme = 0;
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
	change_directory(view, dir);
}

static int
undo_exec(const char *cmd)
{
	return background_and_wait_for_errors((char *)cmd);
}

static void
parse_args(int argc, char *argv[], const char *dir, char *lwin_path,
		char *rwin_path)
{
	int x;

	/* Get Command Line Arguments */
	for(x = 1; x < argc; x++)
	{
		if(argv[x] == NULL)
		{
			continue;
		}
		else if(!strcmp(argv[x], "-f"))
		{
			cfg.vim_filter = 1;
		}
		else if(!strcmp(argv[x], "--version") || !strcmp(argv[x], "-v"))
		{
			endwin();
			show_version_msg();
			exit(0);
		}
		else if(!strcmp(argv[x], "--help") || !strcmp(argv[x], "-h"))
		{
			endwin();
			show_help_msg();
			exit(0);
		}
		else if(!strcmp(argv[x], "--logging"))
		{
			init_logger(1);
		}
		else if(is_dir(argv[x]))
		{
			if(lwin_path[0] != '\0')
			{
				if(argv[x][0] == '/')
					snprintf(rwin_path, PATH_MAX, "%s", argv[x]);
				else
				{
					char buf[PATH_MAX];
					snprintf(buf, sizeof(buf), "%s/%s", dir, argv[x]);
					canonicalize_path(buf, rwin_path, PATH_MAX);
				}
			}
			else
			{
				if(argv[x][0] == '/')
					snprintf(lwin_path, PATH_MAX, "%s", argv[x]);
				else
				{
					char buf[PATH_MAX];
					snprintf(buf, sizeof(buf), "%s/%s", dir, argv[x]);
					canonicalize_path(buf, lwin_path, PATH_MAX);
				}
			}
		}
		else
		{
			endwin();
			show_help_msg();
			exit(0);
		}
	}
}

int
main(int argc, char *argv[])
{
	char dir[PATH_MAX];
	char config_dir[PATH_MAX];
	char *console = NULL;
	char lwin_path[PATH_MAX] = "";
	char rwin_path[PATH_MAX] = "";

	setlocale(LC_ALL, "");
	if(getcwd(dir, sizeof(dir)) == NULL)
	{
		perror("getcwd");
		return -1;
	}

	init_window(&rwin);
	init_window(&lwin);

	command_list = NULL;
	filetypes = NULL;

	col_schemes = malloc(sizeof(Col_scheme) * 8);

  init_registers();
	init_config();
	set_config_dir();

	read_config_file();

	/* Safety check for existing vifmrc file without FUSE_HOME */
	if(cfg.fuse_home == NULL)
		cfg.fuse_home = strdup("/tmp/vifm_FUSE");

	/* Misc configuration */

	lwin.prev_invert = lwin.invert;
	lwin.hide_dot = 1;
	strncpy(lwin.regexp, "", sizeof(lwin.regexp));
	init_window_history(&lwin);

	rwin.prev_invert = rwin.invert;
	rwin.hide_dot = 1;
	strncpy(rwin.regexp, "", sizeof(rwin.regexp));
	init_window_history(&rwin);

	init_status();
	curr_stats.dirsize_cache = tree_create();
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

	if(calculate_crc32(config_dir, &curr_stats.config_crc32) != 0)
		curr_stats.config_crc32 = 0;

	/* Check if running in X */
	console = getenv("DISPLAY");
	if(!console || !*console)
		curr_stats.is_console = 1;

	/* Setup the ncurses interface. */
	if(!setup_ncurses_interface())
		return -1;

	curr_view = &lwin;
	other_view = &rwin;

	read_info_file();

	parse_args(argc, argv, dir, lwin_path, rwin_path);
	if(lwin_path[0] != '\0')
		strcpy(lwin.curr_dir, lwin_path);
	if(rwin_path[0] != '\0')
		strcpy(rwin.curr_dir, rwin_path);

	load_initial_directory(&lwin, dir);
	load_initial_directory(&rwin, dir);

	init_modes();
	init_option_handlers();
	init_undo_list(&undo_exec, &cfg.undo_levels);
	load_local_options(curr_view);

	curr_stats.vifm_started = 1;

	exec_startup();

	moveto_list_pos(&lwin, 0);
	update_all_windows();
	setup_signals();

	mvwaddstr(rwin.win, rwin.curr_line, 0, "*");
	wrefresh(rwin.win);

	werase(status_bar);
	wnoutrefresh(status_bar);

	/* Need to wait until both lists are loaded before changing one of the
	 * lists to show the file stats.  This is only used for starting vifm
	 * from the vifm.vim script
	 * TODO understand why we need to wait
	 */

	if(cfg.vim_filter)
		curr_stats.number_of_windows = 1;

	curr_stats.vifm_started = 2;
	redraw_window();

	main_loop();

	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
