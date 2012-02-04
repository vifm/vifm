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

#ifndef __UI_H__
#define __UI_H__

#ifdef _WIN32
#include <windows.h>
#endif

#include <limits.h> /* PATH_MAX NAME_MAX */
#include <curses.h>
#include <stdlib.h> /* off_t mode_t... */
#include <inttypes.h> /* uintmax_t */
#include <sys/types.h>
#include <unistd.h>
/* For Solaris */
#ifndef NAME_MAX
#include <dirent.h>
#ifndef MAXNAMLEN
#define MAXNAMLEN FILENAME_MAX
#endif
#define NAME_MAX MAXNAMLEN
#endif

#include "color_scheme.h"

#define MIN_TERM_HEIGHT 10
#define MIN_TERM_WIDTH 30

#define SORT_WIN_WIDTH 32

enum
{
	SORT_BY_EXTENSION = 1,
	SORT_BY_NAME,
#ifndef _WIN32
	SORT_BY_GROUP_ID,
	SORT_BY_GROUP_NAME,
	SORT_BY_MODE,
	SORT_BY_OWNER_ID,
	SORT_BY_OWNER_NAME,
#endif
	SORT_BY_SIZE,
	SORT_BY_TIME_ACCESSED,
	SORT_BY_TIME_CHANGED,
	SORT_BY_TIME_MODIFIED,
	SORT_BY_INAME,
	NUM_SORT_OPTIONS = SORT_BY_INAME
};

typedef struct
{
	int rel_pos;
	char dir[PATH_MAX];
	char file[NAME_MAX];
}history_t;

typedef struct
{
	char *name;
	unsigned long long size;
#ifndef _WIN32
	uid_t uid;
	gid_t gid;
	mode_t mode;
#else
	DWORD attrs;
#endif
	time_t mtime;
	time_t atime;
	time_t ctime;
	char *owner;
	char *group;
	char date[16];
	char type;
	int selected;
	int search_match;
	char executable;
	int list_num;
	char ext[24];
}dir_entry_t;

typedef struct _FileView
{
	WINDOW *win;
	WINDOW *title;
	char curr_dir[PATH_MAX];
#ifndef _WIN32
	time_t dir_mtime;
#else
	FILETIME dir_mtime;
	HANDLE dir_watcher;
	char watched_dir[PATH_MAX];
#endif
	char last_dir[PATH_MAX];

	char regexp[256]; /* regular expression pattern for / and ? searching */
	int matches;

	int hide_dot;
	int prev_invert;
	int invert; /* whether to invert the filename pattern */
	int curr_line; /* current line # of the window  */
	int top_line; /* # of the list position that is the top line in window */
	int list_pos; /* actual position in the file list */
	int list_rows; /* size of the file list */
	int window_rows; /* number of rows shown in window */
	unsigned int window_width;
	int filtered;  /* number of files filtered out and not shown in list */
	int selected_files;
	int color_scheme; /* current color scheme being used */
	dir_entry_t *dir_entry;
	char ** selected_filelist;
	int nsaved_selection;
	char ** saved_selection;
	int user_selection;
	int explore_mode; /* shows whether this view is used for file exploring */

	char * prev_filter;
	char * filename_filter; /* regexp for filtering files in dir list */

	char sort[NUM_SORT_OPTIONS];

	int history_num;
	int history_pos;
	history_t *history;

	col_scheme_t cs;
}FileView;

FileView lwin;
FileView rwin;
FileView *other_view;
FileView *curr_view;

WINDOW *status_bar;
WINDOW *stat_win;
WINDOW *pos_win;
WINDOW *input_win;
WINDOW *menu_win;
WINDOW *sort_win;
WINDOW *change_win;
WINDOW *error_win;
WINDOW *top_line;

WINDOW *lborder;
WINDOW *mborder;
WINDOW *rborder;

void is_term_working(void);
int setup_ncurses_interface(void);
void update_stat_window(FileView *view);
float get_splitter_pos(int max);
void redraw_window(void);
void update_pos_window(FileView *view);
void status_bar_messagef(const char *format, ...);
void status_bar_message(const char *message);
void status_bar_error(const char *message);
void status_bar_errorf(const char *message, ...);
int is_status_bar_multiline(void);
void clean_status_bar(void);
void change_window(void);
void update_all_windows(void);
void update_input_bar(wchar_t *str);
void clear_num_window(void);
void show_progress(const char *msg, int period);
void redraw_lists(void);
int load_color_scheme(const char *name);
void wprint(WINDOW *win, const char *str);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
