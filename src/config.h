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

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <curses.h>

#include <limits.h>

typedef struct _Config {
	char home_dir[PATH_MAX]; /* ends with a slash */
	char config_dir[PATH_MAX];
	char trash_dir[PATH_MAX];
	char log_file[PATH_MAX];
	char *vi_command;
	int vi_cmd_bg;
	char *vi_x_command;
	int vi_x_cmd_bg;
	int num_bookmarks; /* Number of active bookmarks (set at the moment) */
	int use_trash;
	int vim_filter;
	int use_screen;
	int use_vim_help;
	int command_num;
	int filetypes_num;
	int xfiletypes_num;
	int fileviewers_num;
	int history_len;
	int nmapped_num;
	int screen_num;

	int auto_execute;
	int show_one_window;
	long max_args;
	int use_iec_prefixes;
	int wrap_quick_view;
	char *time_format;
	char *fuse_home;

	char **search_history;
	int search_history_len;
	int search_history_num;

	char **cmd_history;
	int cmd_history_len;
	int cmd_history_num;

	char **prompt_history;
	int prompt_history_len;
	int prompt_history_num;

	int color_scheme_num;
	int color_scheme_cur;
	int color_scheme;

	int undo_levels; /* Maximum number of changes that can be undone. */

	int sort_numbers; /* Natural sort of (version) numbers within text. */

	int follow_links; /* Follow links on l or Enter. */

	int confirm; /* Ask user about permanent deletion of files. */

	int fast_run;

	int invert_cur_line;

	int wild_menu;

	int ignore_case;
	int smart_case;

	int hl_search;

	int vifm_info;

	int auto_ch_pos;

	char *shell;

	int timeout_len;

	int scroll_off;
} Config;

extern Config cfg;

void load_default_configuration(void);
void read_info_file(int reread);
void write_config_file(void);
void write_info_file(void);
void set_config_dir(void);
void init_config(void);
void exec_config(void);
void source_file(const char *file);
int is_old_config(void);
const char * get_vicmd(int *bg);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
