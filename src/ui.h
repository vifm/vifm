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

#ifndef VIFM__UI_H__
#define VIFM__UI_H__

#ifdef _WIN32
#include <windows.h>
#endif

#include <curses.h>
#include <stddef.h> /* size_t */
#include <stdint.h> /* uint64_t */
#include <stdlib.h> /* off_t mode_t... */
#include <inttypes.h> /* uintmax_t */
#include <sys/types.h>
#include <unistd.h>

#include "utils/filter.h"
#include "utils/fs_limits.h"
#include "color_scheme.h"
#include "column_view.h"
#include "status.h"
#include "types.h"

#define MIN_TERM_HEIGHT 10
#define MIN_TERM_WIDTH 30

#define SORT_WIN_WIDTH 32

/* Width of the input window (located to the left of position window). */
#define INPUT_WIN_WIDTH 6

/* Width of the position window (located in the right corner of status line). */
#define POS_WIN_WIDTH 13

/* Width of the position and input windows. */
#define FIELDS_WIDTH (INPUT_WIN_WIDTH + POS_WIN_WIDTH)

/* New values should be added at the end of enumeration to do not brake sort
 * settings stored in vifminfo files.  Also LAST_SORT_OPTION and
 * SORT_OPTION_COUNT should be updated accordingly. */
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
#ifndef _WIN32
	SORT_BY_PERMISSIONS,
#endif

	/* Default sort key. */
#ifndef _WIN32
	DEFAULT_SORT_KEY = SORT_BY_NAME,
#else
	DEFAULT_SORT_KEY = SORT_BY_INAME,
#endif
	/* Value of the last sort option. */
#ifndef _WIN32
	LAST_SORT_OPTION = SORT_BY_PERMISSIONS,
#else
	LAST_SORT_OPTION = SORT_BY_INAME,
#endif
	/* Number of sort options. */
	SORT_OPTION_COUNT = LAST_SORT_OPTION,
	/* Special value to use for unset options. */
	NO_SORT_OPTION = LAST_SORT_OPTION + 1
};

typedef struct
{
	int rel_pos;
	char *dir;
	char *file;
}history_t;

typedef struct
{
	char *name;
	uint64_t size;
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
	char date[16];
	FileType type;
	int selected;
	int search_match;
	int list_num; /* to be used by sorting comparer to perform stable sort */
}dir_entry_t;

typedef struct
{
	WINDOW *win;
	WINDOW *title;
	char curr_dir[PATH_MAX];
#ifndef _WIN32
#ifdef HAVE_STRUCT_STAT_ST_MTIM
	struct timespec dir_mtime;
#else
	time_t dir_mtime;
#endif
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

	/* Filter which is controlled by user. */
	filter_t name_filter;
	/* Stores previous raw value of the name_filter to make filter restoring
	 * possible.  Not NULL. */
	char *prev_name_filter;

	/* Filter which is controlled automatically and never filled by user. */
	filter_t auto_filter;
	/* Stores previous raw value of the name_filter to make filter restoring
	 * possible.  Not NULL. */
	char *prev_auto_filter;

	/* Various parameters related to local filter. */
	struct
	{
		/* Local filename filter. */
		filter_t filter;
		/* Whether interactive filtering in progress. */
		int in_progress;
		/* Removed value of local filename filter.  Stored for restore operation. */
		char *prev;
		/* Temporary storage for local filename filter, when its overwritten. */
		char *saved;

		/* Unfiltered file entries. */
		dir_entry_t *unfiltered;
		/* Number of unfiltered entries. */
		size_t unfiltered_count;

		/* List of previous cursor positions in the unfiltered array. */
		int *poshist;
		/* Number of elements in the poshist field. */
		size_t poshist_len;
	}
	local_filter;

	char sort[SORT_OPTION_COUNT];

	int history_num;
	int history_pos;
	history_t *history;

	col_scheme_t cs;

	columns_t columns; /* handle for column_view unit */
	char *view_columns; /* format string of columns */

	/* ls-like view related fields */
	int ls_view; /* non-zero if ls-like view is enabled */
	size_t max_filename_len; /* max length of filename in current directory */
	size_t column_count; /* number of columns in the view, used for list view */
	size_t window_cells; /* max number of files that can be displayed */
}
FileView;

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
/* Redraws whole screen with possible reloading of file lists (depends on
 * argument). */
void update_screen(UpdateType update_kind);
void update_pos_window(FileView *view);
/* Sets text to be displayed in position window (ruler).  Real window update is
 * postponed for efficiency reasons. */
void ui_pos_window_set(const char val[]);
void status_bar_messagef(const char *format, ...);
void status_bar_message(const char *message);
void status_bar_error(const char *message);
void status_bar_errorf(const char *message, ...);
int is_status_bar_multiline(void);
void clean_status_bar(void);
void change_window(void);
void update_all_windows(void);
void update_input_bar(const wchar_t *str);
void clear_num_window(void);
void show_progress(const char *msg, int period);
void redraw_lists(void);
/* Returns new value for curr_stats.save_msg. */
int load_color_scheme(const char name[]);
void wprint(WINDOW *win, const char *str);
/* Sets inner flag or signals about needed view update in some other way.
 * It doesn't perform any update, just request one to happen in the future. */
void request_view_update(FileView *view);
/* Performs resizing of some of TUI elements for menu like modes. */
void resize_for_menu_like(void);
/* Performs real pane redraw in the TUI and maybe some related operations. */
void refresh_view_win(FileView *view);
/* Layouts the view in correct corner with correct relative position
 * (horizontally/vertically, left-top/right-bottom). */
void move_window(FileView *view, int horizontally, int first);
/* Switches two panes saving current windows as the active one (left/top or
 * right/bottom). */
void switch_windows(void);
/* Swaps current and other views. */
void switch_panes(void);
/* Switches to other pane, ignoring state of the preview and entering view mode
 * in case the other pane has explore mode active. */
void go_to_other_pane(void);
/* Splits windows according to the value of orientation. */
void split_view(SPLIT orientation);
/* Switches view to one-window mode. */
void only(void);
/* File name formatter which takes 'classify' option into account and applies
 * type dependent name decorations. */
void format_entry_name(FileView *view, size_t pos, size_t buf_len, char buf[]);
/* Moves cursor to position specified by coordinates checking result of the
 * movement. */
void checked_wmove(WINDOW *win, int y, int x);
/* Notifies TUI module about updated window of the view. */
void ui_view_win_changed(FileView *view);

#endif /* VIFM__UI_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
