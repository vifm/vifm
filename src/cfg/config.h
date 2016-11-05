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

#ifndef VIFM__CFG__CONFIG_H__
#define VIFM__CFG__CONFIG_H__

#include <stddef.h> /* size_t wchar_t */

#include "../compat/fs_limits.h"
#include "../ui/color_scheme.h"
#include "../ui/ui.h"
#include "../types.h"
#include "hist.h"

/* Name of help file in plain text format. */
#define VIFM_HELP "vifm-help.txt"

/* Name help file in Vim-documentation format. */
#define VIFM_VIM_HELP "vifm-app.txt"

/* Name of directory in main configuration directory that contains scripts
 * implicitly included into $PATH for Vifm only. */
#define SCRIPTS_DIR "scripts"

/* Path to global directory for color schemes. */
#define GLOBAL_COLORS_DIR PACKAGE_SYSCONF_DIR "/colors/"

typedef enum
{
	DD_ROOT_PARENT    = 1 << 0,
	DD_NONROOT_PARENT = 1 << 1,
	NUM_DOT_DIRS      =      2
}
DotDirs;

/* Tweaks of case sensitivity. */
typedef enum
{
	CO_PATH_COMPL = 1 << 0, /* Completion of paths. */
	CO_GOTO_FILE  = 1 << 1, /* Navigation to files via f/F/,/;. */
	NUM_CASE_OPTS =      2  /* Number of case options for compile checks. */
}
CaseOpts;

/* Possible flags that regulate key suggestions. */
typedef enum
{
	SF_NORMAL            = 1 << 0, /* Display in normal mode. */
	SF_VISUAL            = 1 << 1, /* Display in visual mode. */
	SF_VIEW              = 1 << 2, /* Display in view mode. */
	SF_OTHERPANE         = 1 << 3, /* Display list in other pane, if available. */
	SF_DELAY             = 1 << 4, /* Postpone suggestions by small delay. */
	SF_KEYS              = 1 << 5, /* Include keys suggestions in results. */
	SF_MARKS             = 1 << 6, /* Include marks suggestions in results. */
	SF_REGISTERS         = 1 << 7, /* Include registers suggestions in results. */
	NUM_SUGGESTION_FLAGS =      8  /* Number of flags. */
}
SuggestionFlags;

/* What should be displayed as size of a directory in a view by default. */
typedef enum
{
	VDS_SIZE,   /* Size of a directory (not its content). */
	VDS_NITEMS, /* Number of items in a directory. */
}
ViewDirSize;

/* Indexes for cfg.type_decs. */
enum
{
	DECORATION_PREFIX, /* The symbol, which is prepended to file name. */
	DECORATION_SUFFIX, /* The symbol, which is appended to file name. */
};

/* Operations that require user confirmation. */
enum
{
	CONFIRM_DELETE      = 1, /* Deletion to trash. */
	CONFIRM_PERM_DELETE = 2, /* Permanent deletion. */
};

/* Custom view configuration. */
enum
{
	CVO_AUTOCMDS    = 1, /* Trigger autocommands on entering/leaving [v]cv. */
	CVO_LOCALOPTS   = 2, /* Reset local options on entering/leaving [v]cv. */
	CVO_LOCALFILTER = 4, /* Reset local filter on entering/leaving [v]cv. */
};

/* File decoration description. */
typedef struct
{
	struct matchers_t *matchers; /* Name matcher object. */
	char prefix[9];              /* File name prefix. */
	char suffix[9];              /* File name suffix. */
}
file_dec_t;

typedef struct config_t
{
	char home_dir[PATH_MAX];   /* Ends with a slash. */
	char config_dir[PATH_MAX]; /* Where local configuration files are stored. */
	char colors_dir[PATH_MAX]; /* Where local color files are stored. */
	char data_dir[PATH_MAX];   /* Where to store data files. */

	/* This one should be set using set_trash_dir() function. */
	char trash_dir[PATH_MAX];
	char log_file[PATH_MAX];
	char *vi_command;
	int vi_cmd_bg;
	char *vi_x_command;
	int vi_x_cmd_bg;
	int use_trash;

	/* Whether support of terminal multiplexers is enabled. */
	int use_term_multiplexer;

	int use_vim_help;
	int history_len;

	int auto_execute;
	int use_iec_prefixes;
	int wrap_quick_view;
	char *time_format;
	/* This one should be set using cfg_set_fuse_home() function. */
	char *fuse_home;

	/* History of command-line commands. */
	hist_t cmd_hist;
	/* History of search patterns. */
	hist_t search_hist;
	/* History of prompt input. */
	hist_t prompt_hist;
	/* History of local filter patterns. */
	hist_t filter_hist;

	col_scheme_t cs;

	int undo_levels;  /* Maximum number of changes that can be undone. */
	int sort_numbers; /* Natural sort of (version) numbers within text. */
	int follow_links; /* Follow links on l or Enter. */
	int fast_run;

	int confirm; /* About which operations user should be asked (CONFIRM_*). */

	/* Whether wild menu should be used. */
	int wild_menu;
	/* Whether wild menu should be a popup instead of a bar. */
	int wild_popup;

	/* Settings related to suggestions. */
	struct
	{
		/* Combination of SuggestionFlags, configures when and how to show
		 * suggestions. */
		int flags;
		/* Maximum number of register files to display in suggestions. */
		int maxregfiles;
		/* Delay before displaying suggestions (in milliseconds). */
		int delay;
	}
	sug;

	int ignore_case;
	int smart_case;
	int hl_search;
	int vifm_info;
	int auto_ch_pos;
	char *shell;
	int scroll_off;
	int gdefault;
	int scroll_bind;
	int wrap_scan;
	int inc_search;
	int selection_is_primary; /* For yy, dd and DD: act on selection not file. */
	int tab_switches_pane; /* Whether <tab> is switch pane or history forward. */
	int use_system_calls; /* Prefer performing operations with system calls. */
	int tab_stop;
	char *ruler_format;
	char *status_line;
	int lines; /* Terminal height in lines. */
	int columns; /* Terminal width in characters. */
	/* Controls displaying of dot directories.  Combination of DotDirs flags. */
	int dot_dirs;
	/* File type specific prefixes and suffixes ('classify'). */
	char type_decs[FT_COUNT][2][9];
	/* File name specific prefixes and suffixes ('classify'). */
	file_dec_t *name_decs;
	/* Size of name_decs array. */
	int name_dec_count;
	int filter_inverted_by_default; /* Default inversion value for :filter. */

	/* Invocation formats for external applications. */
	char *apropos_prg; /* apropos tool calling pattern. */
	char *find_prg;    /* find tool calling pattern. */
	char *grep_prg;    /* grep tool calling pattern. */
	char *locate_prg;  /* locate tool calling pattern. */
	char *delete_prg;  /* File removal application. */

	/* Message shortening controlled by 'shortmess'. */
	int trunc_normal_sb_msgs; /* Truncate normal status bar messages if needed. */
	int shorten_title_paths;  /* Use tilde shortening in view titles. */

	/* Comma-separated list of file system types which are slow to respond. */
	char *slow_fs_list;

	/* Coma separated list of places to look for relative path to directories. */
	char *cd_path;

	/* Whether there should be reserved single character width space before and
	 * after file list column inside a view and first and last columns and lines
	 * for a quick view. */
	int extra_padding;

	/* Whether side borders are visible (separator in the middle isn't
	 * affected). */
	int side_borders_visible;

	/* Whether statusline is visible. */
	int display_statusline;

	/* Per line pattern for borders. */
	char *border_filler;

	/* Whether terminal title should be updated or not. */
	int set_title;

	/* Whether directory path should always be resolved to real path (all symbolic
	 * link expanded). */
	int chase_links;

	int timeout_len;     /* Maximum period on waiting for the input. */
	int min_timeout_len; /* Minimum period on waiting for the input. */

	char word_chars[256]; /* Whether corresponding character is a word char. */

	ViewDirSize view_dir_size; /* Type of size display for directories in view. */

	/* Controls use of fast file cloning for file systems that support it. */
	int fast_file_cloning;

	/* Whether various things should be reset on entering/leaving custom views. */
	int cvoptions;

	/* Tweaks of case sensitivity.  Values of these variables change the default
	 * behaviour with regard to case sensitivity of various aspects.  These are
	 * bit sets of CO_* flags. */
	int case_override; /* Flag set here means the fact of the override. */
	int case_ignore;   /* Flag here means case should be either always ignored or
	                      always respected. */
}
config_t;

extern config_t cfg;

/* Initializes cfg global variable with initial values.  Re-initialization is
 * not supported. */
void cfg_init(void);

/* Searches for configuration file and directories, stores them and ensures
 * existence of some of them.  This routine is separated from cfg_init() to
 * allow logging of path discovery. */
void cfg_discover_paths(void);

/* Sources vifmrc file (pointed to by the $MYVIFMRC). */
void cfg_load(void);

/* Returns non-zero on error. */
int cfg_source_file(const char filename[]);

/* Gets editor invocation command.  Sets *bg to indicate whether the command
 * should be executed in background.  Returns pointer to a string from
 * configuration variables. */
const char * cfg_get_vicmd(int *bg);

/* Changes size of all histories.  Zero or negative length disables history. */
void cfg_resize_histories(int new_len);

/* Sets value of cfg.fuse_home.  Returns non-zero in case of error, otherwise
 * zero is returned. */
int cfg_set_fuse_home(const char new_value[]);

/* Sets whether support of terminal multiplexers is enabled. */
void cfg_set_use_term_multiplexer(int use_term_multiplexer);

/* Frees memory previously allocated for specified history items. */
void cfg_free_history_items(const history_t history[], size_t len);

/* Saves command to command history. */
void cfg_save_command_history(const char command[]);

/* Saves pattern to search history. */
void cfg_save_search_history(const char pattern[]);

/* Saves input to prompt history. */
void cfg_save_prompt_history(const char input[]);

/* Saves input to local filter history. */
void cfg_save_filter_history(const char pattern[]);

/* Gets the most recently used search pattern.  Returns the pattern or empty
 * string if search history is empty. */
const char * cfg_get_last_search_pattern(void);

/* Sets shell invocation command. */
void cfg_set_shell(const char shell[]);

/* Checks whether given wide character should be considered as part of a word
 * according to current settings.  Returns non-zero if so, otherwise zero is
 * returned. */
int cfg_is_word_wchar(wchar_t c);

/* Whether ../ directory should appear in file list.  Returns non-zero if so,
 * and zero otherwise. */
int cfg_parent_dir_is_visible(int in_root);

/* Checks whether file deletion (possibly into trash) requires user confirmation
 * according to current configuration.  Returns non-zero if so, otherwise zero
 * is returned. */
int cfg_confirm_delete(int to_trash);

#endif /* VIFM__CFG__CONFIG_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
