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

#include <sys/types.h> /* ino_t */

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
#include "../status.h"
#include "../types.h"
#include "color_scheme.h"

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
#ifndef _WIN32
	SK_BY_INODE,          /* Inode number. */
#endif
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
#ifndef _WIN32
	SK_LAST = SK_BY_INODE,
#else
	SK_LAST = SK_BY_TARGET,
#endif

	/* Number of sort options. */
	SK_COUNT = SK_LAST,

	/* Special value to use for unset options. */
	SK_NONE = SK_LAST + 1,

	/* Defined here to do not add it to sorting keys. */
	/* Id ordering. */
	SK_BY_ID = SK_NONE + 1,
	/* Displaying root of file name only. */
	SK_BY_ROOT,

	/* Total number of SK_*, including those missing from sorting keys and other
	 * counts. */
	SK_TOTAL
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

/* Variants of custom view. */
typedef enum
{
	CV_REGULAR,     /* Sorted list of files. */
	CV_VERY,        /* No initial sorting of file list is enforced. */
	CV_TREE,        /* Files of a file system sub-tree. */
	CV_CUSTOM_TREE, /* Selected files of a file system sub-tree. */
	CV_COMPARE,     /* Directory comparison pane. */
	CV_DIFF,        /* One of two directory comparison panes. */
}
CVType;

/* Type of file comparison. */
typedef enum
{
	CT_NAME,     /* Compare just names. */
	CT_SIZE,     /* Compare file sizes. */
	CT_CONTENTS, /* Compare file contents by combining size and hash. */
}
CompareType;

/* Type of scheduled view update event. */
typedef enum
{
	UUE_NONE,   /* No even scheduled at the time of request. */
	UUE_REDRAW, /* View redraw. */
	UUE_RELOAD, /* View reload with saving selection and cursor. */
}
UiUpdateEvent;

/* Defines the way entry name should be formatted. */
typedef enum
{
	NF_NONE, /* No formatting at all. */
	NF_ROOT, /* Exclude extension and decorate the rest. */
	NF_FULL  /* Decorate the whole name. */
}
NameFormat;

typedef struct
{
	int rel_pos;
	char *dir;
	char *file;
}
history_t;

/* Enable forward declaration of dir_entry_t. */
typedef struct dir_entry_t dir_entry_t;
/* Description of a single directory entry. */
struct dir_entry_t
{
	char *name;       /* File name. */
	char *origin;     /* Location where this file comes from.  Points to
	                     view::curr_dir for non-cv views, otherwise allocated on
	                     a heap. */
	uint64_t size;    /* File size in bytes. */
#ifndef _WIN32
	uid_t uid;        /* Owning user id. */
	gid_t gid;        /* Owning group id. */
	mode_t mode;      /* Mode of the file. */
	ino_t inode;      /* Inode number. */
#else
	uint32_t attrs;   /* Attributes of the file. */
#endif
	time_t mtime;     /* Modification time. */
	time_t atime;     /* Access time. */
	time_t ctime;     /* Creation time. */
	FileType type;    /* File type. */
	int nlinks;       /* Number of hard links to the entry. */

	int id;           /* File uniqueness identifier on comparison. */

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
	unsigned int dir_link : 1;     /* Whether this is symlink to a directory. */
};

/* List of entries bundled with its size. */
typedef struct
{
	dir_entry_t *entries; /* List of entries. */
	int nentries;         /* Number entries in the list. */
}
entries_t;

/* Data related to custom filling. */
struct cv_data_t
{
	/* Type of the custom view. */
	CVType type;

	/* Additional data about CV_DIFF type. */
	CompareType diff_cmp_type; /* Type of comparison. */
	int diff_path_group;       /* Whether entries are grouped by paths. */

	/* This is temporary storage for custom list entries used during its
	 * construction. */
	dir_entry_t *entries; /* File entries. */
	int entry_count;      /* Number of file entries. */

	/* Title of the custom view being constructed.  Discarded if finishing
	 * fails. */
	char *next_title;

	/* Directory we were in before custom view activation. */
	char *orig_dir;
	/* Title for the custom view. */
	char *title;

	/* Previous sorting value, before unsorted custom view was loaded. */
	char sort[SK_COUNT];

	/* List of paths that should be ignored (including all nested paths).  Used
	 * by tree-view. */
	struct trie_t *excluded_paths;

	/* Names of files in custom view while it's being composed.  Used for
	 * duplicate elimination during construction of custom list. */
	struct trie_t *paths_cache;
};

/* Various parameters related to local filter. */
struct local_filter_t
{
	/* Original list of custom entries saved because otherwise we lose it. */
	dir_entry_t *entries; /* File entries. */
	int entry_count;      /* Number of file entries. */

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
};

/* Cached file list coupled with a watcher. */
typedef struct
{
	fswatch_t *watch;  /* Watcher for the path. */
	char *dir;         /* Path to watched directory. */
	entries_t entries; /* Cached list of entries. */
}
cached_entries_t;

/* Enable forward declaration of view_t. */
typedef struct view_t view_t;
/* State of a pane. */
struct view_t
{
	WINDOW *win;
	WINDOW *title;

	/* Directory we're currently in. */
	char curr_dir[PATH_MAX];

	/* Data related to custom filling. */
	struct cv_data_t custom;

	/* Various parameters related to local filter. */
	struct local_filter_t local_filter;

	/* Non-zero if miller columns view is enabled. */
	int miller_view, miller_view_g;
	/* Proportions of columns. */
	int miller_ratios[3], miller_ratios_g[3];
	/* Caches of file lists for miller mode. */
	cached_entries_t left_column;
	cached_entries_t right_column;

	/* Monitor that checks for directory changes. */
	fswatch_t *watch;
	char watched_dir[PATH_MAX];

	char last_dir[PATH_MAX];

	/* Number of files that match current search pattern. */
	int matches;
	/* Last used search pattern, empty if none. */
	char last_search[NAME_MAX + 1];

	int hide_dot, hide_dot_g; /* Whether dot files are hidden. */
	int prev_invert;
	int invert; /* whether to invert the filename pattern */
	int curr_line; /* current line # of the window  */
	int top_line; /* # of the list position that is the top line in window */
	int list_pos; /* actual position in the file list */
	int list_rows; /* size of the file list */
	int window_rows; /* Number of rows in the window. */
	int window_cols; /* Number of columns in the window. */
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

	int explore_mode;       /* Whether this view is used for file exploring. */
	struct view_info_t *vi; /* State of explore view (NULL initially). */

	/* Filter which is controlled by user. */
	struct matcher_t *manual_filter;
	/* Stores previous raw value of the manual_filter to make filter restoring
	 * possible.  Always not NULL. */
	char *prev_manual_filter;

	/* Filter which is controlled automatically and never filled by user. */
	filter_t auto_filter;
	/* Stores previous raw value of the auto_filter to make filter restoring
	 * possible.  Not NULL. */
	char *prev_auto_filter;

	/* List of sorting keys. */
	char sort[SK_COUNT], sort_g[SK_COUNT];
	/* Sorting groups (comma-separated list of regular expressions). */
	char *sort_groups, *sort_groups_g;
	/* Primary group in compiled form. */
	regex_t primary_group;

	int history_num;    /* Number of used history elements. */
	int history_pos;    /* Current position in history. */
	history_t *history; /* Directory history itself. */

	col_scheme_t cs;

	/* Handle for column_view unit.  Contains view columns configuration even when
	 * 'lsview' is on. */
	struct columns_t *columns;
	/* Format string that specifies view columns. */
	char *view_columns, *view_columns_g;

	/* Preview command to use instead of programs configured via :fileviewer. */
	char *preview_prg, *preview_prg_g;

	/* ls-like view related fields. */
	int ls_view, ls_view_g;             /* Non-zero if ls-like view is enabled. */
	int ls_transposed, ls_transposed_g; /* Non-zero for transposed ls-view. */
	size_t max_filename_width; /* Maximum filename width (length in character
	                            * positions on the screen) among all entries of
	                            * the file list.  Zero if not calculated. */
	int column_count; /* Number of columns in the view, used for list view. */
	int run_size;     /* Length of run in leading direction (neighbourhood),
	                   * number of elements in a stride.  Greater than 1 in
	                   * ls-like/grid view. */
	int window_cells; /* Max number of files that can be displayed. */

	/* Whether and how line numbers are displayed. */
	NumberingType num_type, num_type_g;
	/* Min number of characters reserved for number field. */
	int num_width, num_width_g;
	int real_num_width; /* Real character count reserved for number field. */

	/* Timestamps for controlling of scheduling update requests.  They are in
	 * microseconds.  Real resolution is bigger than microsecond, but it's not
	 * critical. */

	uint64_t postponed_redraw;         /* Time of last redraw request. */
	uint64_t postponed_reload;         /* Time of last reload request. */
	pthread_mutex_t *timestamps_mutex; /* Protects access to above variables.
	                                      This is a pointer, because mutexes
	                                      shouldn't be copied*/

	uint64_t last_redraw; /* Time of last redraw. */
	uint64_t last_reload; /* Time of last [full] reload. */

	int on_slow_fs; /* Whether current directory has access penalties. */
};

extern view_t lwin;
extern view_t rwin;
view_t *other_view;
view_t *curr_view;

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
WINDOW *tab_line;

WINDOW *lborder;
WINDOW *mborder;
WINDOW *rborder;

/* Updates the ruler with information from the view (possibly lazily). */
void ui_ruler_update(view_t *view, int lazy_redraw);

/* Sets text to be displayed on the ruler.  Real window update is postponed for
 * efficiency reasons. */
void ui_ruler_set(const char val[]);

/* Checks whether terminal is operational and has at least minimal
 * dimensions.  Might terminate application on unavailable terminal.  Updates
 * term_state in status structure. */
void ui_update_term_state(void);

/* Checks whether given character was pressed, ignores any other characters. */
int ui_char_pressed(wint_t c);

int setup_ncurses_interface(void);

/* Closes current tab if it's not the last one, closes whole application
 * otherwise.  Might also fail if background tasks are present and user chooses
 * not to stop them. */
void ui_quit(int write_info, int force);

/* Checks whether custom view of specified type is unsorted.  Returns non-zero
 * if so, otherwise zero is returned. */
int cv_unsorted(CVType type);

/* Checks whether custom view of specified type is a compare or diff view.
 * Returns non-zero if so, otherwise zero is returned. */
int cv_compare(CVType type);

/* Checks whether custom view of specified type is a tree view.  Returns
 * non-zero if so, otherwise zero is returned. */
int cv_tree(CVType type);

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
 * its absolute value is used and count is not printed. */
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

/* Performs updates of layout for menu like modes. */
void ui_setup_for_menu_like(void);

/* Performs real pane redraw in the TUI and maybe some related operations. */
void refresh_view_win(view_t *view);

/* Layouts the view in correct corner with correct relative position
 * (horizontally/vertically, left-top/right-bottom). */
void move_window(view_t *view, int horizontally, int first);

/* Swaps current and other views. */
void switch_panes(void);

/* Swaps data fields of two panes.  Doesn't correct origins of directory
 * entries. */
void ui_swap_view_data(view_t *left, view_t *right);

/* Setups view to the the curr_view.  Saving previous state in supplied buffers.
 * Use ui_view_unpick() to revert the effect. */
void ui_view_pick(view_t *view, view_t **old_curr, view_t **old_other);

/* Restores what has been done by ui_view_pick(). */
void ui_view_unpick(view_t *view, view_t *old_curr, view_t *old_other);

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
void ui_view_resize(view_t *view, int to);

/* File name formatter which takes 'classify' option into account and applies
 * type dependent name decorations if requested. */
void format_entry_name(const dir_entry_t *entry, NameFormat fmt, size_t buf_len,
		char buf[]);

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
void ui_view_win_changed(view_t *view);

/* Resets selection of the view and reloads it preserving cursor position. */
void ui_view_reset_selection_and_reload(view_t *view);

/* Resets search highlighting of the view and schedules reload. */
void ui_view_reset_search_highlight(view_t *view);

/* Reloads visible lists of files preserving current position of cursor. */
void ui_views_reload_visible_filelists(void);

/* Reloads lists of files preserving current position of cursor. */
void ui_views_reload_filelists(void);

/* Updates title of the views. */
void ui_views_update_titles(void);

/* Updates title of the view. */
void ui_view_title_update(view_t *view);

/* Looks for the given key in sort option.  Returns non-zero when found,
 * otherwise zero is returned. */
int ui_view_sort_list_contains(const char sort[SK_COUNT], char key);

/* Ensures that list of sorting keys is sensible (i.e. contains either "name" or
 * "iname" for views, except for unsorted custom view). */
void ui_view_sort_list_ensure_well_formed(view_t *view, char sort_keys[]);

/* Picks sort array for the view taking custom view into account.  sort should
 * point to sorting array preferred by default.  Returns pointer to the
 * array. */
char * ui_view_sort_list_get(const view_t *view, const char sort[]);

/* Checks whether file numbers should be displayed for the view.  Returns
 * non-zero if so, otherwise zero is returned. */
int ui_view_displays_numbers(const view_t *view);

/* Checks whether view is visible on the screen.  Returns non-zero if so,
 * otherwise zero is returned. */
int ui_view_is_visible(const view_t *view);

/* Checks whether view displays column view.  Returns non-zero if so, otherwise
 * zero is returned. */
int ui_view_displays_columns(const view_t *view);

/* Gets real type of file view entry.  Returns type of entry, resolving symbolic
 * link if needed. */
FileType ui_view_entry_target_type(const dir_entry_t *entry);

/* Gets width of part of the view that is available for file list.  Returns the
 * width. */
int ui_view_available_width(const view_t *view);

/* Retrieves width reserved for something to the left of file list.  Returns the
 * width. */
int ui_view_left_reserved(const view_t *view);

/* Retrieves width reserved for something to the right of file list.  Returns
 * the width. */
int ui_view_right_reserved(const view_t *view);

/* Retrieves column number at which quickview content should be displayed.
 * Returns the number. */
int ui_qv_left(const view_t *view);

/* Retrieves line number at which quickview content should be displayed.
 * Returns the number. */
int ui_qv_top(const view_t *view);

/* Retrieves height of quickview area.  Returns the height. */
int ui_qv_height(const view_t *view);

/* Retrieves width of quickview area.  Returns the width. */
int ui_qv_width(const view_t *view);

/* Gets color scheme that corresponds to the view.  Returns pointer to the color
 * scheme. */
const col_scheme_t * ui_view_get_cs(const view_t *view);

/* Erases view window by filling it with the background color. */
void ui_view_erase(view_t *view);

/* Same as erase, but ensures that view is updated in all its size on the
 * screen (e.g. to clear anything put there by other programs as well). */
void ui_view_wipe(view_t *view);

/* Checks whether custom view type of specified view is unsorted.  It doesn't
 * need the view to be custom, checks just the type.  Returns non-zero if so,
 * otherwise zero is returned. */
int ui_view_unsorted(const view_t *view);

/* Shuts down UI making it possible to use terminal (either after vifm is closed
 * or when terminal might be used by another application that vifm runs). */
void ui_shutdown(void);

/* View update scheduling. */

/* Schedules redraw of the view for the future.  Doesn't perform any actual
 * update. */
void ui_view_schedule_redraw(view_t *view);

/* Schedules reload of the view for the future.  Doesn't perform any actual
 * work. */
void ui_view_schedule_reload(view_t *view);

/* Clears previously scheduled redraw request of the view, if any. */
void ui_view_redrawn(view_t *view);

/* Checks for scheduled update and marks it as fulfilled.  Returns kind of
 * scheduled event. */
UiUpdateEvent ui_view_query_scheduled_event(view_t *view);

#endif /* VIFM__UI__UI_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
