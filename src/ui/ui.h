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

#ifndef VIFM__UI__UI_H__
#define VIFM__UI__UI_H__

#include <sys/types.h>

#include <curses.h>
#include <regex.h> /* regex_t */

#include <stddef.h> /* size_t wchar_t */
#include <stdint.h> /* uint64_t uint32_t */
#include <stdlib.h> /* mode_t */
#include <time.h> /* time_t */
#include <wchar.h> /* wint_t */

#include "../compat/fs_limits.h"
#include "../compat/pthread.h"
#include "../utils/filter.h"
#include "../utils/fswatch.h"
#include "../utils/trie.h"
#include "../status.h"
#include "../types.h"
#include "color_scheme.h"
#include "column_view.h"

#define SORT_WIN_WIDTH 32

/* Width of the input window (located to the left of the ruler). */
#define INPUT_WIN_WIDTH 6

/* Minimal width of the position window (located in the right corner of status
 * line). */
#define POS_WIN_MIN_WIDTH 13

/* Menus don't look like menus as all if height is less than 5. */
#define MIN_TERM_HEIGHT 5
/* There is a lower limit on statusbar width. */
#define MIN_TERM_WIDTH (INPUT_WIN_WIDTH + 1 + POS_WIN_MIN_WIDTH)

/* Width of the ruler and input windows. */
#define FIELDS_WIDTH() (INPUT_WIN_WIDTH + getmaxx(ruler_win))

/* New values should be added at the end of enumeration to do not brake sort
 * settings stored in vifminfo files.  Also SK_LAST and SK_COUNT should be
 * updated accordingly. */
typedef enum
{
	SK_BY_EXTENSION = 1,  /* Extension of files and directories. */
	SK_BY_NAME,           /* Name (including extension). */
#ifndef _WIN32
	SK_BY_GROUP_ID,       /* Group id. */
	SK_BY_GROUP_NAME,     /* Group name. */
	SK_BY_MODE,           /* File mode (file type + permissions) in octal. */
	SK_BY_OWNER_ID,       /* Owner id. */
	SK_BY_OWNER_NAME,     /* Owner name. */
#endif
	SK_BY_SIZE,           /* Size. */
	SK_BY_TIME_ACCESSED,  /* Time accessed (e.g. read, executed). */
	SK_BY_TIME_CHANGED,   /* Time changed (changes in metadata, e.g. mode). */
	SK_BY_TIME_MODIFIED,  /* Time modified (when file contents is changed). */
	SK_BY_INAME,          /* Name (including extension, ignores case). */
#ifndef _WIN32
	SK_BY_PERMISSIONS,    /* Permissions string. */
#endif
	SK_BY_DIR,            /* Directory grouping (directory < file). */
	SK_BY_TYPE,           /* File type (dir/reg/exe/link/char/block/sock/fifo). */
	SK_BY_FILEEXT,        /* Extension of files only. */
	SK_BY_NITEMS,         /* Number of items in a directory (zero for files). */
	SK_BY_GROUPS,         /* Groups extracted via regexps from 'sortgroups'. */
#ifndef _WIN32
	SK_BY_NLINKS,         /* Number of hard links. */
#endif
	SK_BY_TARGET,         /* Symbolic link target (empty for other file types). */
	/* New elements *must* be added here to keep values stored in existing
	 * vifminfo files valid.  Don't forget to update SK_LAST below. */
}
SortingKey;

enum
{
	/* Default sort key. */
#ifndef _WIN32
	SK_DEFAULT = SK_BY_NAME,
#else
	SK_DEFAULT = SK_BY_INAME,
#endif

	/* Value of the last sort option. */
	SK_LAST = SK_BY_TARGET,

	/* Number of sort options. */
	SK_COUNT = SK_LAST,

	/* Special value to use for unset options. */
	SK_NONE = SK_LAST + 1
};

/* Type of file numbering. */
typedef enum
{
	NT_NONE = 0x00,            /* Displaying of file numbers is disabled. */
	NT_SEQ  = 0x01,            /* Number are displayed as is (sequentially). */
	NT_REL  = 0x02,            /* Relative numbers are used for all items. */
	NT_MIX  = NT_SEQ | NT_REL, /* All numbers are relative except for current. */
}
NumberingType;

/* Type of scheduled view update event. */
typedef enum
{
	UUE_NONE,        /* No even scheduled at the time of request. */
	UUE_REDRAW,      /* View redraw. */
	UUE_RELOAD,      /* View reload with saving selection and cursor. */
	UUE_FULL_RELOAD, /* Full view reload. */
}
UiUpdateEvent;

typedef struct
{
	int rel_pos;
	char *dir;
	char *file;
}
history_t;

typedef struct dir_entry_t
{
	char *name;
	char *origin;     /* Location where this file comes from. */
	uint64_t size;
#ifndef _WIN32
	uid_t uid;
	gid_t gid;
	mode_t mode;
#else
	uint32_t attrs;
#endif
	time_t mtime;
	time_t atime;
	time_t ctime;
	FileType type;
	int nlinks;       /* Number of hard links to the entry. */

	int tag;          /* Used to hold temporary data associated with the item,
	                     e.g. by sorting comparer to perform stable sort or item
	                     mapping during tree filtering. */

	int hi_num;       /* File highlighting parameters cache (initially -1). */
	int name_dec_num; /* File decoration parameters cache (initially -1).  The
	                     value is shifted by one, 0 means type decoration. */

	int child_count; /* Number of child entries (all, not just direct). */
	int child_pos;   /* Position of this entry in among children of its parent.
	                    Zero for top-level entries. */

	int search_match;      /* Non-zero if the item matches last search.  Equals to
	                          search match number (top to bottom order). */
	short int match_left;  /* Starting position of search match. */
	short int match_right; /* Ending position of search match. */

	unsigned int selected : 1;     /* Whether file is selected. */
	unsigned int was_selected : 1; /* Previous selection state for Visual mode. */
	unsigned int marked : 1;       /* Whether file should be processed. */
	unsigned int temporary : 1;    /* Whether this is temporary node. */
}
dir_entry_t;

typedef struct
{
	WINDOW *win;
	WINDOW *title;

	/* Directory we're currently in. */
	char curr_dir[PATH_MAX];

	/* Parameters related to custom filling. */
	struct
	{
		/* Type of the custom view. */
		enum
		{
			CV_REGULAR, /* Sorted list of files. */
			CV_VERY,    /* No initial sorting of file list is enforced. */
			CV_TREE,    /* Files of a file system sub-tree. */
		}
		type;

		/* This is temporary storage for custom list entries used during its
		 * construction as well as storage for unfiltered custom list if local
		 * filter is not empty. */
		dir_entry_t *entries; /* File entries. */
		int entry_count;      /* Number of file entries. */

		/* Directory we were in before custom view activation. */
		char *orig_dir;
		/* Title for the custom view. */
		char *title;

		/* Previous sorting value, before unsorted custom view was loaded. */
		char sort[SK_COUNT];

		/* List of paths that should be ignored (including all nested paths).  Used
		 * by tree-view. */
		trie_t excluded_paths;

		/* Names of files in custom view while it's being composed.  Used for
		 * duplicate elimination during construction of custom list. */
		trie_t paths_cache;
	}
	custom;

	/* Monitor that checks for directory changes. */
	fswatch_t *watch;
	char watched_dir[PATH_MAX];

	char last_dir[PATH_MAX];

	/* Number of files that match current search pattern. */
	int matches;
	/* Last used search pattern, empty if none. */
	char last_search[NAME_MAX];

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
	int selected_files; /* Number of currently selected files. */
	int local_cs; /* Whether directory-specific color scheme is in use. */
	dir_entry_t *dir_entry; /* Must be handled via dynarray unit. */

	int nsaved_selection;   /* Number of items in saved_selection. */
	char **saved_selection; /* Names of selected files. */

	/* Whether files are selected by user or via range on the command-line.  Say
	 * some commands (e.g. ga/gA) implement "smart selection", but it shouldn't
	 * be applied for selected produced by a range. */
	int user_selection;

	int explore_mode; /* shows whether this view is used for file exploring */

	/* Filter which is controlled by user. */
	filter_t manual_filter;
	/* Stores previous raw value of the manual_filter to make filter restoring
	 * possible.  Not NULL. */
	char *prev_manual_filter;

	/* Filter which is controlled automatically and never filled by user. */
	filter_t auto_filter;
	/* Stores previous raw value of the auto_filter to make filter restoring
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
		/* Number of entries filtered in other ways. */
		size_t prefiltered_count;

		/* List of previous cursor positions in the unfiltered array. */
		int *poshist;
		/* Number of elements in the poshist field. */
		size_t poshist_len;
	}
	local_filter;

	/* List of sorting keys. */
	char sort[SK_COUNT], sort_g[SK_COUNT];
	/* Sorting groups (comma-separated list of regular expressions). */
	char *sort_groups, *sort_groups_g;
	/* Primary group in compiled form. */
	regex_t primary_group;

	int history_num;
	int history_pos;
	history_t *history;

	col_scheme_t cs;

	/* Handle for column_view unit.  Contains view columns configuration even when
	 * 'lsview' is on. */
	columns_t columns;
	/* Format string that specifies view columns. */
	char *view_columns, *view_columns_g;

	/* ls-like view related fields */
	int ls_view, ls_view_g; /* Non-zero if ls-like view is enabled. */
	size_t max_filename_width; /* Maximum filename width (length in character
	                            * positions on the screen) among all entries of
	                            * the file list.  Zero if not calculated. */
	size_t column_count; /* number of columns in the view, used for list view */
	size_t window_cells; /* max number of files that can be displayed */

	/* Whether and how line numbers are displayed. */
	NumberingType num_type, num_type_g;
	/* Min number of characters reserved for number field. */
	int num_width, num_width_g;
	int real_num_width; /* Real character count reserved for number field. */

	/* Timestamps for controlling of scheduling update requests.  They are in
	 * microseconds.  Real resolution is bigger than microsecond, but it's not
	 * critical. */

	uint64_t postponed_redraw;         /* Time of last redraw request. */
	uint64_t postponed_reload;         /* Time of last redraw request. */
	uint64_t postponed_full_reload;    /* Time of last full redraw request. */
	pthread_mutex_t *timestamps_mutex; /* Protects access to above variables.
	                                      This is a pointer, because mutexes
	                                      shouldn't be copied*/

	uint64_t last_redraw; /* Time of last redraw. */
	uint64_t last_reload; /* Time of last [full] reload. */

	int on_slow_fs; /* Whether current directory has access penalties. */
}
FileView;

FileView lwin;
FileView rwin;
FileView *other_view;
FileView *curr_view;

WINDOW *status_bar;
WINDOW *stat_win;
WINDOW *job_bar;
WINDOW *ruler_win;
WINDOW *input_win;
WINDOW *menu_win;
WINDOW *sort_win;
WINDOW *change_win;
WINDOW *error_win;
WINDOW *top_line;

WINDOW *lborder;
WINDOW *mborder;
WINDOW *rborder;

/* Updates the ruler with information from the view. */
void ui_ruler_update(FileView *view);

/* Sets text to be displayed on the ruler.  Real window update is postponed for
 * efficiency reasons. */
void ui_ruler_set(const char val[]);

/* Checks whether terminal is operational and has at least minimal
 * dimensions.  Might terminate application on unavailable terminal.  Updates
 * term_state in status structure. */
void ui_update_term_state(void);

/* Performs a quick check whether terminal is functional.  Returns non-zero if
 * so, otherwise zero is returned. */
int ui_term_is_alive(void);

/* Checks whether given character was pressed, ignores any other characters. */
int ui_char_pressed(wint_t c);

int setup_ncurses_interface(void);

float get_splitter_pos(int max);

/* Redraws whole screen with possible reloading of file lists (depends on
 * argument). */
void update_screen(UpdateType update_kind);

/* Swaps curr_view and other_view pointers (active and inactive panes).  Also
 * updates things (including UI) that are bound to views. */
void change_window(void);

/* Swaps curr_view and other_view pointers. */
void swap_view_roles(void);

/* Forcibly updates all windows. */
void update_all_windows(void);

/* Touches all windows, actual update can be performed later. */
void touch_all_windows(void);

void update_input_bar(const wchar_t *str);

void clear_num_window(void);

/* Displays progress on the status bar, not updating it frequently.  msg can't
 * be NULL.  period - how often status bar should be updated.  If period equals
 * 0 inner counter is reset, do this on start of operation.  For period <= 1,
 * it's absolute value is used and count is not printed. */
void show_progress(const char msg[], int period);

void redraw_lists(void);

/* Forces immediate update of attributes for most of windows. */
void update_attributes(void);

/* Prints str in current window position. */
void wprint(WINDOW *win, const char str[]);

/* Prints str in current window position with specified line attributes, which
 * set during print operation only. */
void wprinta(WINDOW *win, const char str[], int line_attrs);

/* Performs resizing of some of TUI elements for menu like modes.  Returns zero
 * on success, and non-zero otherwise. */
int resize_for_menu_like(void);

/* Performs real pane redraw in the TUI and maybe some related operations. */
void refresh_view_win(FileView *view);

/* Layouts the view in correct corner with correct relative position
 * (horizontally/vertically, left-top/right-bottom). */
void move_window(FileView *view, int horizontally, int first);

/* Swaps current and other views. */
void switch_panes(void);

/* Setups view to the the curr_view.  Saving previous state in supplied
 * buffers.  Use ui_view_unpick() to revert the effect. */
void ui_view_pick(FileView *view, FileView **old_curr, FileView **old_other);

/* Restores what has been done by ui_view_pick(). */
void ui_view_unpick(FileView *view, FileView *old_curr, FileView *old_other);

/* Switches to other pane, ignoring state of the preview and entering view mode
 * in case the other pane has explore mode active. */
void go_to_other_pane(void);

/* Splits windows according to the value of orientation. */
void split_view(SPLIT orientation);

/* Switches view to one-window mode. */
void only(void);

/* Moves window splitter by specified amount of positions multiplied by the
 * given factor. */
void move_splitter(int by, int fact);

/* Sets size of the view to specified value. */
void ui_view_resize(FileView *view, int to);

/* File name formatter which takes 'classify' option into account and applies
 * type dependent name decorations. */
void format_entry_name(const dir_entry_t *entry, size_t buf_len, char buf[]);

/* Retrieves decorations for file entry.  Sets *prefix and *suffix to strings
 * stored in global configuration. */
void ui_get_decors(const dir_entry_t *entry, const char **prefix,
		const char **suffix);

/* Moves cursor to position specified by coordinates checking result of the
 * movement. */
void checked_wmove(WINDOW *win, int y, int x);

/* Displays "Terminal is too small" kind of message instead of UI. */
void ui_display_too_small_term_msg(void);

/* Notifies TUI module about updated window of the view. */
void ui_view_win_changed(FileView *view);

/* Resets selection of the view and reloads it preserving cursor position. */
void ui_view_reset_selection_and_reload(FileView *view);

/* Resets search highlighting of the view and schedules reload. */
void ui_view_reset_search_highlight(FileView *view);

/* Reloads visible lists of files preserving current position of cursor. */
void ui_views_reload_visible_filelists(void);

/* Reloads lists of files preserving current position of cursor. */
void ui_views_reload_filelists(void);

/* Updates title of the views. */
void ui_views_update_titles(void);

/* Updates title of the view. */
void ui_view_title_update(FileView *view);

/* Looks for the given key in sort option.  Returns non-zero when found,
 * otherwise zero is returned. */
int ui_view_sort_list_contains(const char sort[SK_COUNT], char key);

/* Ensures that list of sorting keys is sensible (i.e. contains either "name" or
 * "iname" for views, except for unsorted custom view). */
void ui_view_sort_list_ensure_well_formed(FileView *view, char sort_keys[]);

/* Picks sort array for the view taking custom view into account.  Returns
 * pointer to the array. */
char * ui_view_sort_list_get(const FileView *view);

/* Checks whether file numbers should be displayed for the view.  Returns
 * non-zero if so, otherwise zero is returned. */
int ui_view_displays_numbers(const FileView *const view);

/* Checks whether view is visible on the screen.  Returns non-zero if so,
 * otherwise zero is returned. */
int ui_view_is_visible(const FileView *const view);

/* Cleans directory history of the view. */
void ui_view_clear_history(FileView *const view);

/* Checks whether view displays column view.  Returns non-zero if so, otherwise
 * zero is returned. */
int ui_view_displays_columns(const FileView *const view);

/* Gets real type of file view entry.  Returns type of entry, resolving symbolic
 * link if needed. */
FileType ui_view_entry_target_type(const dir_entry_t *entry);

/* Gets width of part of the view that is available for file list.  Returns the
 * width. */
int ui_view_available_width(const FileView *const view);

/* Retrieves column number at which quickview content should be displayed.
 * Returns the number. */
int ui_qv_left(const FileView *view);

/* Retrieves line number at which quickview content should be displayed.
 * Returns the number. */
int ui_qv_top(const FileView *view);

/* Retrieves height of quickview area.  Returns the height. */
int ui_qv_height(const FileView *view);

/* Retrieves width of quickview area.  Returns the width. */
int ui_qv_width(const FileView *view);

/* Gets color scheme that corresponds to the view.  Returns pointer to the color
 * scheme. */
const col_scheme_t * ui_view_get_cs(const FileView *view);

/* Erases view window by filling it with the background color. */
void ui_view_erase(FileView *view);

/* Same as erase, but ensures that view is updated in all its size on the
 * screen (e.g. to clear anything put there by other programs as well). */
void ui_view_wipe(FileView *view);

/* View update scheduling. */

/* Schedules redraw of the view for the future.  Doesn't perform any actual
 * update. */
void ui_view_schedule_redraw(FileView *view);

/* Schedules reload of the view for the future.  Doesn't perform any actual
 * work. */
void ui_view_schedule_reload(FileView *view);

/* Schedules full reload of the view for the future.  Doesn't perform any actual
 * work. */
void ui_view_schedule_full_reload(FileView *view);

/* Clears previously scheduled redraw request of the view, if any. */
void ui_view_redrawn(FileView *view);

/* Checks for scheduled update and marks it as fulfilled.  Returns kind of
 * scheduled event. */
UiUpdateEvent ui_view_query_scheduled_event(FileView *view);

#endif /* VIFM__UI__UI_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
