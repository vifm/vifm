/* vifm
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

#include "opt_handlers.h"

#include <curses.h> /* stdscr wnoutrefresh() */
#include <regex.h> /* regex_t regcomp() regexec() regfree() */

#include <assert.h> /* assert() */
#include <ctype.h> /* isdigit() */
#include <limits.h> /* INT_MIN */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* abs() free() */
#include <string.h> /* memcpy() memmove() strchr() strdup() strlen() strncat()
                       strstr() */

#include "cfg/config.h"
#include "engine/options.h"
#include "engine/text_buffer.h"
#include "int/term_title.h"
#include "modes/view.h"
#include "ui/fileview.h"
#include "ui/quickview.h"
#include "ui/statusbar.h"
#include "ui/ui.h"
#include "utils/darray.h"
#include "utils/log.h"
#include "utils/macros.h"
#include "utils/matchers.h"
#include "utils/regexp.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/utils.h"
#include "filelist.h"
#include "flist_hist.h"
#include "search.h"
#include "sort.h"
#include "status.h"
#include "trash.h"
#include "types.h"
#include "viewcolumns_parser.h"

/* TODO: provide default primitive type based handlers (see *prg_handler). */

/* Default value of 'viewcolumns' option, used when it's empty. */
#define DEFAULT_VIEW_COLUMNS "-{name},{}"

typedef union
{
	int *bool_val;
	int *int_val;
	char **str_val;
	int *enum_item;
	int *set_items;
}
optvalref_t;

typedef void (*optinit_func)(optval_t *val);

typedef struct
{
	optvalref_t ref;
	optinit_func init;
}
optinit_t;

static void uni_handler(const char name[], optval_t val, OPT_SCOPE scope);
static void init_classify(optval_t *val);
static char * double_commas(const char str[]);
static void init_cpoptions(optval_t *val);
static void init_dirsize(optval_t *val);
static const char * to_endpoint(int i, char buffer[]);
static void init_timefmt(optval_t *val);
static void init_trashdir(optval_t *val);
static void init_dotfiles(optval_t *val);
static void init_lsview(optval_t *val);
static void init_shortmess(optval_t *val);
static void init_iooptions(optval_t *val);
static void init_number(optval_t *val);
static void init_numberwidth(optval_t *val);
static void init_relativenumber(optval_t *val);
static void init_sort(optval_t *val);
static void init_sortorder(optval_t *val);
static void init_tuioptions(optval_t *val);
static void init_wordchars(optval_t *val);
static void load_options_defaults(void);
static void add_options(void);
static void load_sort_option_inner(FileView *view, char sort_keys[]);
static void aproposprg_handler(OPT_OP op, optval_t val);
static void autochpos_handler(OPT_OP op, optval_t val);
static void caseoptions_handler(OPT_OP op, optval_t val);
static void cdpath_handler(OPT_OP op, optval_t val);
static void chaselinks_handler(OPT_OP op, optval_t val);
static void classify_handler(OPT_OP op, optval_t val);
static int str_to_classify(const char str[], char type_decs[FT_COUNT][2][9]);
static const char * pick_out_decoration(char classify_item[], FileType *type,
		const char **expr);
static int validate_decorations(const char prefix[], const char suffix[]);
static void free_file_decs(file_dec_t *name_decs, int count);
static void columns_handler(OPT_OP op, optval_t val);
static void confirm_handler(OPT_OP op, optval_t val);
static void cpoptions_handler(OPT_OP op, optval_t val);
static void cvoptions_handler(OPT_OP op, optval_t val);
static void deleteprg_handler(OPT_OP op, optval_t val);
static void dirsize_handler(OPT_OP op, optval_t val);
static void dotdirs_handler(OPT_OP op, optval_t val);
static void fastrun_handler(OPT_OP op, optval_t val);
static void fillchars_handler(OPT_OP op, optval_t val);
static void reset_fillchars(void);
static void findprg_handler(OPT_OP op, optval_t val);
static void followlinks_handler(OPT_OP op, optval_t val);
static void fusehome_handler(OPT_OP op, optval_t val);
static void gdefault_handler(OPT_OP op, optval_t val);
static void grepprg_handler(OPT_OP op, optval_t val);
static void history_handler(OPT_OP op, optval_t val);
static void hlsearch_handler(OPT_OP op, optval_t val);
static void iec_handler(OPT_OP op, optval_t val);
static void ignorecase_handler(OPT_OP op, optval_t val);
static void incsearch_handler(OPT_OP op, optval_t val);
static void iooptions_handler(OPT_OP op, optval_t val);
static void laststatus_handler(OPT_OP op, optval_t val);
static void lines_handler(OPT_OP op, optval_t val);
static void locateprg_handler(OPT_OP op, optval_t val);
static void mintimeoutlen_handler(OPT_OP op, optval_t val);
static void scroll_line_down(FileView *view);
static void rulerformat_handler(OPT_OP op, optval_t val);
static void runexec_handler(OPT_OP op, optval_t val);
static void scrollbind_handler(OPT_OP op, optval_t val);
static void scrolloff_handler(OPT_OP op, optval_t val);
static void shell_handler(OPT_OP op, optval_t val);
static void shortmess_handler(OPT_OP op, optval_t val);
#ifndef _WIN32
static void slowfs_handler(OPT_OP op, optval_t val);
#endif
static void smartcase_handler(OPT_OP op, optval_t val);
static void sortnumbers_handler(OPT_OP op, optval_t val);
static void dotfiles_global(OPT_OP op, optval_t val);
static void dotfiles_local(OPT_OP op, optval_t val);
static void lsview_global(OPT_OP op, optval_t val);
static void lsview_local(OPT_OP op, optval_t val);
static void number_global(OPT_OP op, optval_t val);
static void number_local(OPT_OP op, optval_t val);
static void numberwidth_global(OPT_OP op, optval_t val);
static void numberwidth_local(OPT_OP op, optval_t val);
static void set_numberwidth(FileView *view, int *num_width, int width);
static void relativenumber_global(OPT_OP op, optval_t val);
static void relativenumber_local(OPT_OP op, optval_t val);
static void update_num_type(FileView *view, NumberingType *num_type,
		NumberingType type, int enable);
static void sort_global(OPT_OP op, optval_t val);
static void sort_local(OPT_OP op, optval_t val);
static void set_sort(FileView *view, char sort_keys[], char order[]);
static void sortgroups_global(OPT_OP op, optval_t val);
static void sortgroups_local(OPT_OP op, optval_t val);
static void set_sortgroups(FileView *view, char **opt, char value[]);
static void sorting_changed(FileView *view, int defer_slow);
static void sortorder_global(OPT_OP op, optval_t val);
static void sortorder_local(OPT_OP op, optval_t val);
static void set_sortorder(FileView *view, int ascending, char sort_keys[]);
static void viewcolumns_global(OPT_OP op, optval_t val);
static void viewcolumns_local(OPT_OP op, optval_t val);
static void set_viewcolumns(FileView *view, const char view_columns[]);
static void set_view_columns_option(FileView *view, const char value[],
		int update_ui);
static void add_column(columns_t *columns, column_info_t column_info);
static int map_name(const char name[], void *arg);
static void resort_view(FileView * view);
static void statusline_handler(OPT_OP op, optval_t val);
static void suggestoptions_handler(OPT_OP op, optval_t val);
static int read_int(const char line[], int *i);
static void reset_suggestoptions(void);
static void syscalls_handler(OPT_OP op, optval_t val);
static void tabstop_handler(OPT_OP op, optval_t val);
static void timefmt_handler(OPT_OP op, optval_t val);
static void timeoutlen_handler(OPT_OP op, optval_t val);
static void title_handler(OPT_OP op, optval_t val);
static void trash_handler(OPT_OP op, optval_t val);
static void trashdir_handler(OPT_OP op, optval_t val);
static void tuioptions_handler(OPT_OP op, optval_t val);
static void undolevels_handler(OPT_OP op, optval_t val);
static void vicmd_handler(OPT_OP op, optval_t val);
static void vixcmd_handler(OPT_OP op, optval_t val);
static void vifminfo_handler(OPT_OP op, optval_t val);
static void vimhelp_handler(OPT_OP op, optval_t val);
static void wildmenu_handler(OPT_OP op, optval_t val);
static void wildstyle_handler(OPT_OP op, optval_t val);
static void wordchars_handler(OPT_OP op, optval_t val);
static int parse_range(const char range[], int *from, int *to);
static int parse_endpoint(const char **str, int *endpoint);
static void wrap_handler(OPT_OP op, optval_t val);
static void text_option_changed(void);
static void wrapscan_handler(OPT_OP op, optval_t val);

static const char *sort_enum[] = {
	/* SK_* start with 1. */
	[0] = "",

	[SK_BY_EXTENSION]     = "ext",
	[SK_BY_NAME]          = "name",
	[SK_BY_SIZE]          = "size",
	[SK_BY_TIME_ACCESSED] = "atime",
	[SK_BY_TIME_CHANGED]  = "ctime",
	[SK_BY_TIME_MODIFIED] = "mtime",
	[SK_BY_INAME]         = "iname",
	[SK_BY_DIR]           = "dir",
	[SK_BY_TYPE]          = "type",
	[SK_BY_FILEEXT]       = "fileext",
	[SK_BY_NITEMS]        = "nitems",
	[SK_BY_GROUPS]        = "groups",
	[SK_BY_TARGET]        = "target",
#ifndef _WIN32
	[SK_BY_GROUP_ID]      = "gid",
	[SK_BY_GROUP_NAME]    = "gname",
	[SK_BY_MODE]          = "mode",
	[SK_BY_OWNER_ID]      = "uid",
	[SK_BY_OWNER_NAME]    = "uname",
	[SK_BY_PERMISSIONS]   = "perms",
	[SK_BY_NLINKS]        = "nlinks",
#endif
};
ARRAY_GUARD(sort_enum, 1 + SK_COUNT);

/* Possible values of 'caseoptions'. */
static const char *caseoptions_vals[][2] = {
	{ "pPgG", "all caseoptions values" },
	{ "p", "always ignore case of paths during completion" },
	{ "P", "always match case of paths during completion" },
	{ "g", "always ignore case of characters for f/F/;/," },
	{ "G", "always match case of characters for f/F/;/," },
};
ARRAY_GUARD(caseoptions_vals, 1 + NUM_CASE_OPTS*2);

/* Possible values of 'cpoptions'. */
static const char *cpoptions_vals[][2] = {
	{ "fst", "all cpoptions values" },
	{ "f", "leave files that match filter by default" },
	{ "s", "use selection for yy, dd and DD, when present" },
	{ "t", "switch active pane via <tab>" },
};

/* Possible values of 'cvoptions'. */
static const char *cvoptions_vals[][2] = {
	{ "autocmds", "trigger autocommands on entering/leaving custom views" },
	{ "localopts", "reset local options on entering/leaving custom views" },
	{ "localfilter", "reset local filter on entering/leaving custom views" },
};

/* Possible values of 'confirm'. */
static const char *confirm_vals[][2] = {
	{ "delete",     "confirm moving to trash" },
	{ "permdelete", "confirm permanent removal" },
};

/* Possible values of 'dotdirs'. */
static const char *dotdirs_vals[][2] = {
	{ "rootparent",    "show .. in FS root" },
	{ "nonrootparent", "show .. in non-root directories" },
};
ARRAY_GUARD(dotdirs_vals, NUM_DOT_DIRS);

/* Possible values of 'suggestoptions'. */
static const char *suggestoptions_vals[][2] = {
	{ "normal",    "display in normal mode" },
	{ "visual",    "display in visual mode" },
	{ "view",      "display in view mode" },
	{ "otherpane", "use other pane for suggestions, if available" },
	{ "delay",     "display suggestions after a short delay" },
	{ "keys",      "include keys suggestions in results" },
	{ "marks",     "include marks suggestions in results" },
	{ "registers", "include registers suggestions in results" },
};
ARRAY_GUARD(suggestoptions_vals, NUM_SUGGESTION_FLAGS);

/* Possible flags of 'iooptions'. */
static const char *iooptions_vals[][2] = {
	{ "fastfilecloning", "use COW if FS supports it" },
};

/* Possible flags of 'shortmess' and their count. */
static const char *shortmess_vals[][2] = {
	{ "Tp", "all shortmess values" },
	{ "T", "shorten too long status bar messages" },
	{ "p", "substitute home path with ~ in view title" },
};

/* Possible flags of 'tuioptions' and their count. */
static const char *tuioptions_vals[][2] = {
	{ "ps", "all tuioptions values" },
	{ "p",  "use padding in views and preview" },
	{ "s",  "display side borders" },
};

/* Possible values of 'dirsize' option. */
static const char *dirsize_enum[][2] = {
	{ "size",   "size of directory or its files" },
	{ "nitems", "number of files in directory" },
};

/* Possible keys of 'fillchars' option. */
static const char *fillchars_enum[][2] = {
	{ "vborder:", "filler of vertical borders" },
};

/* Possible values of 'sort' option. */
static const char *sort_types[][2] = {
	{ "ext",  "by file/directory extension" },
	{ "+ext", "by file/directory extension" },
	{ "-ext", "by file/directory extension" },

	{ "name",  "by name (case-sensitive)" },
	{ "+name", "by name (case-sensitive)" },
	{ "-name", "by name (case-sensitive)" },

	{ "size",  "by size" },
	{ "+size", "by size" },
	{ "-size", "by size" },

	{ "atime",  "by access time" },
	{ "+atime", "by access time" },
	{ "-atime", "by access time" },

#ifndef _WIN32
	{ "ctime",  "by change time" },
	{ "+ctime", "by change time" },
	{ "-ctime", "by change time" },
#else
	{ "ctime",  "by creation time" },
	{ "+ctime", "by creation time" },
	{ "-ctime", "by creation time" },
#endif

	{ "mtime",  "by modification time" },
	{ "+mtime", "by modification time" },
	{ "-mtime", "by modification time" },

	{ "iname",  "by name (case-insensitive)" },
	{ "+iname", "by name (case-insensitive)" },
	{ "-iname", "by name (case-insensitive)" },

	{ "dir",  "by `is directory` predicate" },
	{ "+dir", "by `is directory` predicate" },
	{ "-dir", "by `is directory` predicate" },

	{ "type",  "by file type" },
	{ "+type", "by file type" },
	{ "-type", "by file type" },

	{ "fileext",  "by file (no directories) extension" },
	{ "+fileext", "by file (no directories) extension" },
	{ "-fileext", "by file (no directories) extension" },

	{ "nitems",  "by number of files in directory" },
	{ "+nitems", "by number of files in directory" },
	{ "-nitems", "by number of files in directory" },

	{ "groups",  "by 'sortgroups' match" },
	{ "+groups", "by 'sortgroups' match" },
	{ "-groups", "by 'sortgroups' match" },

	{ "target",  "by symbolic link target" },
	{ "+target", "by symbolic link target" },
	{ "-target", "by symbolic link target" },

#ifndef _WIN32
	{ "gid",  "by group ID" },
	{ "+gid", "by group ID" },
	{ "-gid", "by group ID" },

	{ "gname",  "by group name" },
	{ "+gname", "by group name" },
	{ "-gname", "by group name" },

	{ "mode",  "by file mode" },
	{ "+mode", "by file mode" },
	{ "-mode", "by file mode" },

	{ "uid",  "by user ID" },
	{ "+uid", "by user ID" },
	{ "-uid", "by user ID" },

	{ "uname",  "by user name" },
	{ "+uname", "by user name" },
	{ "-uname", "by user name" },

	{ "perms",  "by permissions" },
	{ "+perms", "by permissions" },
	{ "-perms", "by permissions" },

	{ "nlinks",  "by number of hard-links" },
	{ "+nlinks", "by number of hard-links" },
	{ "-nlinks", "by number of hard-links" },
#endif
};
ARRAY_GUARD(sort_types, SK_COUNT*3);

/* Possible values of 'sortorder' option. */
static const char *sortorder_enum[][2] = {
	{ "ascending",  "increasing ordering" },
	{ "descending", "decreasing ordering" },
};

/* Possible values of 'vifminfo' option. */
static const char *vifminfo_set[][2] = {
	{ "options",   "values of options" },
	{ "filetypes", "file associations" },
	{ "commands",  "user-defined :commands" },
	{ "bookmarks", "marks (e.g. 'a)" },
	{ "bmarks",    "bookmarks" },
	{ "tui",       "state of TUI" },
	{ "dhistory",  "directory history" },
	{ "state",     "filters and terminal multiplexer" },
	{ "cs",        "current colorscheme" },
	{ "savedirs",  "restore last visited directory" },
	{ "chistory",  "cmdline history" },
	{ "shistory",  "search history" },
	{ "dirstack",  "directory stack" },
	{ "registers", "contents of registers" },
	{ "phistory",  "prompt history" },
	{ "fhistory",  "local filter history" },
};

/* Possible values of 'wildstyle'. */
static const char *wildstyle_vals[][2] = {
	{ "bar",   "single-line bar" },
	{ "popup", "multi-line popup" },
};
ARRAY_GUARD(wildstyle_vals, 2);

/* Empty value to satisfy default initializer. */
static char empty_val[] = "";
static char *empty = empty_val;

/* Specification of options. */
static struct opt_data_t
{
	const char *name;           /* Full name of the option. */
	const char *abbr;           /* Abbreviated option name. */
	const char *descr;          /* Brief option description. */
	OPT_TYPE type;              /* Option type (boolean, integer, etc.). */
	int val_count;              /* Size of vals. */
	const char *(*vals)[2];     /* Array of possible values. */
	opt_handler global_handler; /* Global option handler. */
	opt_handler local_handler;  /* Local option handler (can be NULL). */
	optinit_t initializer;      /* Definition of source of initial value. */
	optval_t val;               /* Storage for initial value. */
}
options[] = {
	/* Global options. */
	{ "aproposprg", "", ":apropos invocation format",
	  OPT_STR, 0, NULL, &aproposprg_handler, NULL,
	  { .ref.str_val = &cfg.apropos_prg },
	},
	{ "autochpos", "", "restore cursor after cd",
	  OPT_BOOL, 0, NULL, &autochpos_handler, NULL,
	  { .ref.bool_val = &cfg.auto_ch_pos },
	},
	{ "caseoptions", "", "case sensitivity overrides",
	  OPT_CHARSET, ARRAY_LEN(caseoptions_vals), caseoptions_vals,
		&caseoptions_handler, NULL,
	  { .ref.str_val = &empty },
	},
	{ "cdpath", "cd", "list of prefixes for relative cd",
	  OPT_STRLIST, 0, NULL, &cdpath_handler, NULL,
	  { .ref.str_val = &cfg.cd_path },
	},
	{ "chaselinks", "", "resolve links in view path",
	  OPT_BOOL, 0, NULL, &chaselinks_handler, NULL,
	  { .ref.bool_val = &cfg.chase_links },
	},
	{ "classify", "", "file name decorations",
	  OPT_STRLIST, 0, NULL, &classify_handler, NULL,
	  { .init = &init_classify },
	},
	{ "columns", "co", "width of TUI in chars",
	  OPT_INT, 0, NULL, &columns_handler, NULL,
	  { .ref.int_val = &cfg.columns },
	},
	{ "confirm", "cf", "confirm file operations",
	  OPT_SET, ARRAY_LEN(confirm_vals), confirm_vals, &confirm_handler, NULL,
	  { .ref.bool_val = &cfg.confirm },
	},
	{ "cpoptions", "cpo", "compatibility options",
	  OPT_CHARSET, ARRAY_LEN(cpoptions_vals), cpoptions_vals, &cpoptions_handler,
		NULL,
	  { .init = &init_cpoptions },
	},
	{ "cvoptions", "", "configures affects cv enter/leave",
	  OPT_SET, ARRAY_LEN(cvoptions_vals), cvoptions_vals, &cvoptions_handler,
		NULL,
	  { .ref.int_val = &cfg.cvoptions },
	},
	{ "deleteprg", "", "permanent file deletion program",
	  OPT_STR, 0, NULL, &deleteprg_handler, NULL,
	  { .ref.str_val = &empty },
	},
	{ "dirsize", "", "directory size style",
	  OPT_ENUM, ARRAY_LEN(dirsize_enum), dirsize_enum, &dirsize_handler, NULL,
	  { .init = &init_dirsize },
	},
	{ "dotdirs", "", "which dot directories to show",
	  OPT_SET, ARRAY_LEN(dotdirs_vals), dotdirs_vals, &dotdirs_handler, NULL,
	  { .ref.set_items = &cfg.dot_dirs },
	},
	{ "fastrun", "", "autocomplete unambiguous prefixes for :!",
	  OPT_BOOL, 0, NULL, &fastrun_handler, NULL,
	  { .ref.bool_val = &cfg.fast_run },
	},
	{ "fillchars", "fcs", "fillers for interface parts",
		OPT_STRLIST, ARRAY_LEN(fillchars_enum), fillchars_enum, &fillchars_handler,
		NULL,
	  { .ref.str_val = &empty },
	},
	{ "findprg", "", ":find invocation format",
	  OPT_STR, 0, NULL, &findprg_handler, NULL,
	  { .ref.str_val = &cfg.find_prg },
	},
	{ "followlinks", "", "go to target file on opening links",
	  OPT_BOOL, 0, NULL, &followlinks_handler, NULL,
	  { .ref.bool_val = &cfg.follow_links },
	},
	{ "fusehome", "", "base directory for FUSE mounts",
	  OPT_STR, 0, NULL, &fusehome_handler, NULL,
	  { .ref.str_val = &cfg.fuse_home },
	},
	{ "gdefault", "gd", "global :substitute by default",
	  OPT_BOOL, 0, NULL, &gdefault_handler, NULL,
	  { .ref.bool_val = &cfg.gdefault },
	},
	{ "grepprg", "", ":grep invocation format",
	  OPT_STR, 0, NULL, &grepprg_handler, NULL,
	  { .ref.str_val = &cfg.grep_prg },
	},
	{ "history", "hi", "length of all histories",
	  OPT_INT, 0, NULL, &history_handler, NULL,
	  { .ref.int_val = &cfg.history_len },
	},
	{ "hlsearch", "hls", "autoselect search matches",
	  OPT_BOOL, 0, NULL, &hlsearch_handler, NULL,
	  { .ref.bool_val = &cfg.hl_search },
	},
	{ "iec", "", "use IEC notation for sizes",
	  OPT_BOOL, 0, NULL, &iec_handler, NULL,
	  { .ref.bool_val = &cfg.sizefmt.ieci_prefixes },
	},
	{ "ignorecase", "ic", "ignore case on searches",
	  OPT_BOOL, 0, NULL, &ignorecase_handler, NULL,
	  { .ref.bool_val = &cfg.ignore_case },
	},
	{ "incsearch", "is", "incremental (live) search",
	  OPT_BOOL, 0, NULL, &incsearch_handler , NULL,
	  { .ref.bool_val = &cfg.inc_search },
	},
	{ "iooptions", "", "file I/O settings",
	  OPT_SET, ARRAY_LEN(iooptions_vals), iooptions_vals, &iooptions_handler,
		NULL,
	  { .init = &init_iooptions },
	},
	{ "laststatus", "ls", "visibility of status bar",
	  OPT_BOOL, 0, NULL, &laststatus_handler, NULL,
	  { .ref.bool_val = &cfg.display_statusline },
	},
	{ "lines", "", "height of TUI in chars",
	  OPT_INT, 0, NULL, &lines_handler, NULL,
	  { .ref.int_val = &cfg.lines },
	},
	{ "locateprg", "", ":locate invocation format",
	  OPT_STR, 0, NULL, &locateprg_handler, NULL,
	  { .ref.str_val = &cfg.locate_prg },
	},
	{ "mintimeoutlen", "", "delay between input polls",
	  OPT_INT, 0, NULL, &mintimeoutlen_handler, NULL,
	  { .ref.int_val = &cfg.min_timeout_len },
	},
	{ "rulerformat", "ruf", "format of the ruler",
	  OPT_STR, 0, NULL, &rulerformat_handler, NULL,
	  { .ref.str_val = &cfg.ruler_format },
	},
	{ "runexec", "", "run executables on opening them",
	  OPT_BOOL, 0, NULL, &runexec_handler, NULL,
	  { .ref.bool_val = &cfg.auto_execute },
	},
	{ "scrollbind", "scb", "synchronize scroll position in two panes",
	  OPT_BOOL, 0, NULL, &scrollbind_handler, NULL,
	  { .ref.bool_val = &cfg.scroll_bind },
	},
	{ "scrolloff", "so", "minimal cursor distance to view borders",
	  OPT_INT, 0, NULL, &scrolloff_handler, NULL,
	  { .ref.int_val = &cfg.scroll_off },
	},
	{ "shell", "sh", "shell to run external commands",
	  OPT_STR, 0, NULL, &shell_handler, NULL,
	  { .ref.str_val = &cfg.shell },
	},
	{ "shortmess", "shm", "things to shorten in TUI",
	  OPT_CHARSET, ARRAY_LEN(shortmess_vals), shortmess_vals, &shortmess_handler,
		NULL,
	  { .init = &init_shortmess },
	},
#ifndef _WIN32
	{ "slowfs", "", "list of slow filesystem types",
	  OPT_STRLIST, 0, NULL, &slowfs_handler, NULL,
	  { .ref.str_val = &cfg.slow_fs_list },
	},
#endif
	{ "smartcase", "scs", "pick pattern sensitivity based on case",
	  OPT_BOOL, 0, NULL, &smartcase_handler, NULL,
	  { .ref.bool_val = &cfg.smart_case },
	},
	{ "sortnumbers", "", "version sorting for files",
	  OPT_BOOL, 0, NULL, &sortnumbers_handler, NULL,
	  { .ref.bool_val = &cfg.sort_numbers },
	},
	{ "statusline", "stl", "format of the status line",
	  OPT_STR, 0, NULL, &statusline_handler, NULL,
	  { .ref.str_val = &cfg.status_line },
	},
	{ "suggestoptions", "", "when to display key suggestions",
	  OPT_STRLIST, ARRAY_LEN(suggestoptions_vals), suggestoptions_vals,
		&suggestoptions_handler, NULL,
	  { .ref.str_val = &empty },
	},
	{ "syscalls", "", "use system calls for file operations",
	  OPT_BOOL, 0, NULL, &syscalls_handler, NULL,
	  { .ref.bool_val = &cfg.use_system_calls },
	},
	{ "tabstop", "ts", "widths of one tabulation",
	  OPT_INT, 0, NULL, &tabstop_handler, NULL,
	  { .ref.int_val = &cfg.tab_stop },
	},
	{ "timefmt", "", "time format",
	  OPT_STR, 0, NULL, &timefmt_handler, NULL,
	  { .init = &init_timefmt },
	},
	{ "timeoutlen", "tm", "delay on ambiguous key sequence",
	  OPT_INT, 0, NULL, &timeoutlen_handler, NULL,
	  { .ref.int_val = &cfg.timeout_len },
	},
	{ "title", "", "set terminal title",
	  OPT_BOOL, 0, NULL, &title_handler, NULL,
	  { .ref.int_val = &cfg.set_title },
	},
	{ "trash", "", "put files to trash on removing them",
	  OPT_BOOL, 0, NULL, &trash_handler, NULL,
	  { .ref.bool_val = &cfg.use_trash },
	},
	{ "trashdir", "", "path to trash directory",
	  OPT_STRLIST, 0, NULL, &trashdir_handler, NULL,
	  { .init = &init_trashdir },
	},
	{ "tuioptions", "to", "TUI look tweaks",
	  OPT_CHARSET, ARRAY_LEN(tuioptions_vals), tuioptions_vals,
		&tuioptions_handler, NULL,
	  { .init = &init_tuioptions },
	},
	{ "undolevels", "ul", "number of file operations to remember",
	  OPT_INT, 0, NULL, &undolevels_handler, NULL,
	  { .ref.int_val = &cfg.undo_levels },
	},
	{ "vicmd", "", "editor to use in console",
	  OPT_STR, 0, NULL, &vicmd_handler, NULL,
	  { .ref.str_val = &cfg.vi_command },
	},
	{ "vixcmd", "", "editor to use in X",
	  OPT_STR, 0, NULL, &vixcmd_handler, NULL,
	  { .ref.str_val = &cfg.vi_x_command },
	},
	{ "vifminfo", "", "what to store in vifminfo",
	  OPT_SET, ARRAY_LEN(vifminfo_set), vifminfo_set, &vifminfo_handler, NULL,
	  { .ref.set_items = &cfg.vifm_info },
	},
	{ "vimhelp", "", "use Vim help instead of plain text",
	  OPT_BOOL, 0, NULL, &vimhelp_handler, NULL,
	  { .ref.bool_val = &cfg.use_vim_help },
	},
	{ "wildmenu", "wmnu", "display completion matches in TUI",
	  OPT_BOOL, 0, NULL, &wildmenu_handler, NULL,
	  { .ref.bool_val = &cfg.wild_menu },
	},
	{ "wildstyle", "", "how to display completion matches",
	  OPT_ENUM, ARRAY_LEN(wildstyle_vals), wildstyle_vals, &wildstyle_handler,
		NULL,
	  { .ref.enum_item = &cfg.wild_popup },
	},
	{ "wordchars", "", "chars considered to be part of a word",
	  OPT_STRLIST, 0, NULL, &wordchars_handler, NULL,
	  { .init = &init_wordchars },
	},
	{ "wrap", "", "wrap lines in preview",
	  OPT_BOOL, 0, NULL, &wrap_handler, NULL,
	  { .ref.bool_val = &cfg.wrap_quick_view },
	},
	{ "wrapscan", "ws", "wrap search around top/bottom",
	  OPT_BOOL, 0, NULL, &wrapscan_handler, NULL,
	  { .ref.bool_val = &cfg.wrap_scan },
	},

	/* Local options must be grouped here. */
	{ "dotfiles", "", "show dot files",
	  OPT_BOOL, 0, NULL, &dotfiles_global, &dotfiles_local,
	  { .init = &init_dotfiles },
	},
	{ "lsview", "", "display items in a table",
	  OPT_BOOL, 0, NULL, &lsview_global, &lsview_local,
	  { .init = &init_lsview },
	},
	{ "number", "nu", "display line numbers",
	  OPT_BOOL, 0, NULL, &number_global, &number_local,
	  { .init = &init_number },
	},
	{ "numberwidth", "nuw", "minimum width of line number column",
	  OPT_INT, 0, NULL, &numberwidth_global, &numberwidth_local,
	  { .init = &init_numberwidth },
	},
	{ "relativenumber", "rnu", "display relative line numbers",
	  OPT_BOOL, 0, NULL, &relativenumber_global, &relativenumber_local,
	  { .init = &init_relativenumber },
	},
	{ "sort", "", "sequence of sorting keys",
	  OPT_STRLIST, ARRAY_LEN(sort_types), sort_types, &sort_global, &sort_local,
	  { .init = &init_sort },
	},
	{ "sortgroups", "", "regexp for group sorting",
	  OPT_STR, 0, NULL, &sortgroups_global, &sortgroups_local,
	  { .ref.str_val = &empty },
	},
	{ "sortorder", "", "sorting direction of primary key",
	  OPT_ENUM, ARRAY_LEN(sortorder_enum), sortorder_enum, &sortorder_global,
	  &sortorder_local,
	  { .init = &init_sortorder },
	},
	{ "viewcolumns", "", "specification of view columns",
	  OPT_STRLIST, 0, NULL, &viewcolumns_global, &viewcolumns_local,
	  { .ref.str_val = &empty },
	},
};

static int error;

void
init_option_handlers(void)
{
	static int opt_changed;
	init_options(&opt_changed, &uni_handler);
	load_options_defaults();
	add_options();
}

/* Additional handler for processing of global-local options, namely for
 * updating the other view. */
static void
uni_handler(const char name[], optval_t val, OPT_SCOPE scope)
{
	/* += and similar operators act in their way only for the current view, the
	 * other view just gets value resulted from the operation.  The behaviour is
	 * fine and doesn't seem to be a bug. */

	static size_t first_local = (size_t)-1;

	size_t i;

	if(!curr_stats.global_local_settings)
	{
		return;
	}

	/* Find first local option once. */
	if(first_local == (size_t)-1)
	{
		for(first_local = 0U; first_local < ARRAY_LEN(options); ++first_local)
		{
			if(options[first_local].local_handler != NULL)
			{
				break;
			}
		}
	}

	/* Look up option name and update it in the other view if found. */
	for(i = first_local; i < ARRAY_LEN(options); ++i)
	{
		if(strcmp(options[i].name, name) == 0)
		{
			FileView *const tmp_view = curr_view;
			curr_view = other_view;

			/* Make sure option value remains valid even if updated in the handler.
			 * Do this before calling load_view_options(), because it can change the
			 * value. */
			if(ONE_OF(options[i].type, OPT_STR, OPT_STRLIST, OPT_CHARSET))
			{
				val.str_val = strdup(val.str_val);
			}

			load_view_options(curr_view);

			if(scope == OPT_LOCAL)
			{
				options[i].local_handler(OP_SET, val);
			}
			else
			{
				options[i].global_handler(OP_SET, val);
			}

			if(ONE_OF(options[i].type, OPT_STR, OPT_STRLIST, OPT_CHARSET))
			{
				free(val.str_val);
			}

			curr_view = tmp_view;
			load_view_options(curr_view);
			break;
		}
	}
}

/* Composes the default value for the 'classify' option. */
static void
init_classify(optval_t *val)
{
	val->str_val = (char *)classify_to_str();
	if(val->str_val == NULL)
	{
		val->str_val = "";
	}
}

const char *
classify_to_str(void)
{
	static char *classify;
	int ft, i;
	size_t len = 0;
	int memerr = 0;

	if(classify == NULL && (classify = strdup("")) == NULL)
	{
		return NULL;
	}

	classify[0] = '\0';

	/* Name-dependent decorations. */
	for(i = 0; i < cfg.name_dec_count && !memerr; ++i)
	{
		const file_dec_t *const name_dec = &cfg.name_decs[i];
		char *const doubled = double_commas(matchers_get_expr(name_dec->matchers));
		char *const addition = format_str("%s%s::%s::%s",
				classify[0] != '\0' ? "," : "", name_dec->prefix, doubled,
				name_dec->suffix);

		memerr |= (addition == NULL || strappend(&classify, &len, addition) != 0);

		free(addition);
		free(doubled);
	}

	/* Type-dependent decorations. */
	for(ft = 0; ft < FT_COUNT; ++ft)
	{
		const char *const prefix = cfg.type_decs[ft][DECORATION_PREFIX];
		const char *const suffix = cfg.type_decs[ft][DECORATION_SUFFIX];
		if(prefix[0] != '\0' || suffix[0] != '\0')
		{
			char item[64];
			if(classify[0] != '\0')
			{
				memerr |= strappendch(&classify, &len, ',');
			}
			(void)snprintf(item, sizeof(item), "%s:%s:%s", prefix,
					get_type_str(ft), suffix);
			memerr |= strappend(&classify, &len, item);
		}
	}

	return memerr ? NULL : classify;
}

/* Clones string passed as the argument doubling commas in the process.  Returns
 * cloned value. */
static char *
double_commas(const char str[])
{
	char *const doubled = malloc(strlen(str)*2 + 1);
	char *p = doubled;
	while(*str != '\0')
	{
		*p++ = *str++;
		if(p[-1] == ',')
		{
			*p++ = ',';
		}
	}
	*p = '\0';
	return doubled;
}

/* Composes initial value for 'cpoptions' option from a set of configuration
 * flags. */
static void
init_cpoptions(optval_t *val)
{
	static char buf[32];
	/* TODO: move these flags to curr_stats structure, or not why would they fit
	 * there? */
	snprintf(buf, sizeof(buf), "%s%s%s",
			cfg.filter_inverted_by_default ? "f" : "",
			cfg.selection_is_primary       ? "s" : "",
			cfg.tab_switches_pane          ? "t" : "");
	val->str_val = buf;
}

/* Initializes 'dirsize' option from cfg.view_dir_size variable. */
static void
init_dirsize(optval_t *val)
{
	val->enum_item = (cfg.view_dir_size == VDS_SIZE ? 0 : 1);
}

/* Convenience function to format an endpoint inside the buffer, which should be
 * large enough.  Returns the buffer. */
static const char *
to_endpoint(int i, char buffer[])
{
	sprintf(buffer, "%d", i);
	return buffer;
}

static void
init_timefmt(optval_t *val)
{
	val->str_val = cfg.time_format + 1;
}

static void
init_trashdir(optval_t *val)
{
	val->str_val = cfg.trash_dir;
}

/* Initializes 'dotfiles' option from global value. */
static void
init_dotfiles(optval_t *val)
{
	val->bool_val = !curr_view->hide_dot_g;
}

static void
init_lsview(optval_t *val)
{
	val->bool_val = curr_view->ls_view_g;
}

/* Initializes 'shortmess' from current configuration state. */
static void
init_shortmess(optval_t *val)
{
	static char buf[32];
	snprintf(buf, sizeof(buf), "%s%s", cfg.trunc_normal_sb_msgs ? "T" : "",
			cfg.shorten_title_paths ? "p" : "");
	val->str_val = buf;
}

/* Initializes value of 'iooptions' from configuration. */
static void
init_iooptions(optval_t *val)
{
	val->set_items = (cfg.fast_file_cloning != 0) << 0;
}

/* Default-initializes whether to display file numbers. */
static void
init_number(optval_t *val)
{
	val->bool_val = 0;
}

/* Default-initializes minimum width of file number field. */
static void
init_numberwidth(optval_t *val)
{
	val->int_val = 4;
}

/* Default-initializes whether to display relative file numbers. */
static void
init_relativenumber(optval_t *val)
{
	val->bool_val = 0;
}

static void
init_sort(optval_t *val)
{
	val->str_val = "+name";
}

static void
init_sortorder(optval_t *val)
{
	val->enum_item = 0;
}

/* Composes initial value for 'tuioptions' option from a set of configuration
 * flags. */
static void
init_tuioptions(optval_t *val)
{
	static char buf[32];
	snprintf(buf, sizeof(buf), "%s%s",
			cfg.extra_padding ? "p" : "",
			cfg.side_borders_visible ? "s" : "");
	val->str_val = buf;
}

/* Formats 'wordchars' initial value from cfg.word_chars array. */
static void
init_wordchars(optval_t *val)
{
	static char *str;

	size_t i;
	size_t len;

	/* This is for the case when cfg.word_chars is empty, so that we won't return
	 * NULL as initial value. */
	replace_string(&str, "");

	len = 0U;
	i = 0U;
	while(i < sizeof(cfg.word_chars))
	{
		int l, r;

		while(i < sizeof(cfg.word_chars) && !cfg.word_chars[i])
		{
			++i;
		}
		l = i;
		while(i < sizeof(cfg.word_chars) && cfg.word_chars[i])
		{
			++i;
		}
		r = i - 1;

		if((size_t)l < sizeof(cfg.word_chars))
		{
			char left[16];
			char range[32];

			if(len != 0U)
			{
				(void)strappendch(&str, &len, ',');
			}

			if(r == l)
			{
				snprintf(range, sizeof(range), "%s", to_endpoint(l, left));
			}
			else
			{
				char right[16];
				snprintf(range, sizeof(range), "%s-%s", to_endpoint(l, left),
						to_endpoint(r, right));
			}
			(void)strappend(&str, &len, range);
		}
	}

	val->str_val = str;
}

static void
load_options_defaults(void)
{
	size_t i;
	for(i = 0U; i < ARRAY_LEN(options); ++i)
	{
		if(options[i].initializer.init != NULL)
		{
			options[i].initializer.init(&options[i].val);
		}
		else if(ONE_OF(options[i].type, OPT_STR, OPT_STRLIST, OPT_CHARSET))
		{
			options[i].val.str_val = *options[i].initializer.ref.str_val;
		}
		else
		{
			options[i].val.int_val = *options[i].initializer.ref.int_val;
		}
	}
}

/* Registers options in options unit. */
static void
add_options(void)
{
	size_t i;
	for(i = 0U; i < ARRAY_LEN(options); ++i)
	{
		struct opt_data_t *const opt = &options[i];

		add_option(opt->name, opt->abbr, opt->descr, opt->type, OPT_GLOBAL,
				opt->val_count, opt->vals, opt->global_handler, opt->val);

		if(opt->local_handler != NULL)
		{
			add_option(opt->name, opt->abbr, opt->descr, opt->type, OPT_LOCAL,
					opt->val_count, opt->vals, opt->local_handler, opt->val);
		}
	}
}

void
reset_local_options(FileView *view)
{
	optval_t val;

	memcpy(view->sort, view->sort_g, sizeof(view->sort));
	load_sort_option_inner(view, view->sort);

	view->hide_dot = view->hide_dot_g;
	val.int_val = !view->hide_dot_g;
	set_option("dotfiles", val, OPT_LOCAL);

	fview_set_lsview(view, view->ls_view_g);
	val.int_val = view->ls_view_g;
	set_option("lsview", val, OPT_LOCAL);

	view->num_type = view->num_type_g;
	val.bool_val = view->num_type_g & NT_SEQ;
	set_option("number", val, OPT_LOCAL);
	val.bool_val = view->num_type_g & NT_REL;
	set_option("relativenumber", val, OPT_LOCAL);

	view->num_width = view->num_width_g;
	val.int_val = view->num_width_g;
	set_option("numberwidth", val, OPT_LOCAL);

	replace_string(&view->view_columns, view->view_columns_g);
	set_viewcolumns(view, view->view_columns);
	val.str_val = view->view_columns;
	set_option("viewcolumns", val, OPT_LOCAL);

	replace_string(&view->sort_groups, view->sort_groups_g);
	val.str_val = view->sort_groups;
	set_option("sortgroups", val, OPT_LOCAL);
}

void
load_view_options(FileView *view)
{
	optval_t val;

	load_sort_option(view);

	val.str_val = view->view_columns;
	set_option("viewcolumns", val, OPT_LOCAL);
	val.str_val = view->view_columns_g;
	set_option("viewcolumns", val, OPT_GLOBAL);

	val.str_val = view->sort_groups;
	set_option("sortgroups", val, OPT_LOCAL);
	val.str_val = view->sort_groups_g;
	set_option("sortgroups", val, OPT_GLOBAL);

	val.bool_val = !view->hide_dot;
	set_option("dotfiles", val, OPT_LOCAL);
	val.bool_val = !view->hide_dot_g;
	set_option("dotfiles", val, OPT_GLOBAL);

	val.bool_val = view->ls_view;
	set_option("lsview", val, OPT_LOCAL);
	val.bool_val = view->ls_view_g;
	set_option("lsview", val, OPT_GLOBAL);

	val.bool_val = view->num_type & NT_SEQ;
	set_option("number", val, OPT_LOCAL);
	val.bool_val = view->num_type_g & NT_SEQ;
	set_option("number", val, OPT_GLOBAL);

	val.int_val = view->num_width;
	set_option("numberwidth", val, OPT_LOCAL);
	val.int_val = view->num_width_g;
	set_option("numberwidth", val, OPT_GLOBAL);

	val.bool_val = view->num_type & NT_REL;
	set_option("relativenumber", val, OPT_LOCAL);
	val.bool_val = view->num_type_g & NT_REL;
	set_option("relativenumber", val, OPT_GLOBAL);
}

void
clone_local_options(const FileView *from, FileView *to, int defer_slow)
{
	const char *sort;

	replace_string(&to->view_columns, from->view_columns);
	replace_string(&to->view_columns_g, from->view_columns_g);

	to->num_width = from->num_width;
	to->num_width_g = from->num_width_g;

	to->num_type = from->num_type;
	to->num_type_g = from->num_type_g;

	to->hide_dot = from->hide_dot;
	to->hide_dot_g = from->hide_dot_g;

	replace_string(&to->sort_groups, from->sort_groups);
	replace_string(&to->sort_groups_g, from->sort_groups_g);

	sort = (flist_custom_active(from) && ui_view_unsorted(from))
	     ? (char *)from->custom.sort
	     : (char *)from->sort;
	memcpy(to->sort, sort, sizeof(to->sort));
	memcpy(to->sort_g, from->sort_g, sizeof(to->sort_g));
	sorting_changed(to, defer_slow);

	to->ls_view_g = from->ls_view_g;
	fview_set_lsview(to, from->ls_view);
}

void
load_sort_option(FileView *view)
{
	load_sort_option_inner(view, view->sort);
	load_sort_option_inner(view, view->sort_g);

	/* The check is to skip this in tests which don't need columns. */
	if(view->columns != NULL)
	{
		if(!cv_compare(view->custom.type))
		{
			set_viewcolumns(view, view->view_columns);
		}
	}
}

/* Loads sorting related options ("sort" and "sortorder"). */
static void
load_sort_option_inner(FileView *view, char sort_keys[])
{
	/* This approximate maximum length also includes "+" or "-" sign and a
	 * comma (",") between items. */
	enum { MAX_SORT_KEY_LEN = 16 };

	int i;

	optval_t val;
	char opt_val[MAX_SORT_KEY_LEN*SK_COUNT];
	size_t opt_val_len = 0U;
	OPT_SCOPE scope = (sort_keys == view->sort) ? OPT_LOCAL : OPT_GLOBAL;

	if(sort_keys == view->custom.sort)
	{
		val.str_val = "";
		set_option("sort", val, OPT_LOCAL);
		return;
	}

	opt_val[0] = '\0';

	ui_view_sort_list_ensure_well_formed(view, sort_keys);

	/* Produce a string, which represents a list of sorting keys. */
	i = -1;
	while(++i < SK_COUNT && abs(sort_keys[i]) <= SK_LAST)
	{
		const int sort_option = sort_keys[i];
		const char *const comma = (opt_val_len == 0U) ? "" : ",";
		const char option_mark = (sort_option < 0) ? '-' : '+';
		const char *const option_name = sort_enum[abs(sort_option)];

		snprintf(opt_val + opt_val_len, sizeof(opt_val) - opt_val_len, "%s%c%s",
				comma, option_mark, option_name);
		opt_val_len += strlen(opt_val + opt_val_len);
	}

	val.str_val = opt_val;
	set_option("sort", val, scope);

	val.enum_item = (sort_keys[0] < 0);
	set_option("sortorder", val, scope);
}

int
process_set_args(const char args[], int global, int local)
{
	int set_options_error;
	const char *text_buffer;
	OPT_SCOPE scope;

	assert((global || local) && "At least one type of options must be changed.");

	vle_tb_clear(vle_err);

	/* Call of set_options() can change error. */
	error = 0;
	if(local && global)
	{
		scope = OPT_ANY;
	}
	else
	{
		scope = local ? OPT_LOCAL : OPT_GLOBAL;
	}
	set_options_error = (set_options(args, scope) != 0);
	error |= set_options_error;
	text_buffer = vle_tb_get_data(vle_err);

	if(error)
	{
		vle_tb_append_line(vle_err, "Invalid argument for :set command");
		/* We just modified text buffer and can't use text_buffer variable. */
		status_bar_error(vle_tb_get_data(vle_err));
	}
	else if(text_buffer[0] != '\0')
	{
		status_bar_message(text_buffer);
	}

	return error ? -1 : (text_buffer[0] != '\0');
}

static void
aproposprg_handler(OPT_OP op, optval_t val)
{
	(void)replace_string(&cfg.apropos_prg, val.str_val);
}

static void
autochpos_handler(OPT_OP op, optval_t val)
{
	cfg.auto_ch_pos = val.bool_val;
	if(curr_stats.load_stage < 2)
	{
		flist_hist_clear(curr_view);
		flist_hist_clear(other_view);
	}
}

/* Handles changes of 'caseoptions' option.  Updates configuration and
 * normalizes option value. */
static void
caseoptions_handler(OPT_OP op, optval_t val)
{
	char valid_val[32];
	const char *p;

	cfg.case_override = 0;
	cfg.case_ignore = 0;

	p = val.str_val;
	while(*p != '\0')
	{
		if(*p == 'P' || *p == 'p')
		{
			cfg.case_override |= CO_PATH_COMPL;
			cfg.case_ignore = (*p == 'p')
			                ? (cfg.case_ignore | CO_PATH_COMPL)
			                : (cfg.case_ignore & ~CO_PATH_COMPL);
		}
		else if(*p == 'g' || *p == 'G')
		{
			cfg.case_override |= CO_GOTO_FILE;
			cfg.case_ignore = (*p == 'g')
			                ? (cfg.case_ignore | CO_GOTO_FILE)
			                : (cfg.case_ignore & ~CO_GOTO_FILE);
		}

		++p;
	}

	valid_val[0] = '\0';
	if(cfg.case_override & CO_PATH_COMPL)
	{
		strcat(valid_val, (cfg.case_ignore & CO_PATH_COMPL) ? "p" : "P");
	}
	if(cfg.case_override & CO_GOTO_FILE)
	{
		strcat(valid_val, (cfg.case_ignore & CO_GOTO_FILE) ? "g" : "G");
	}
	val.str_val = valid_val;

	set_option("caseoptions", val, OPT_GLOBAL);
}

/* Specifies directories to check on cding by relative path. */
static void
cdpath_handler(OPT_OP op, optval_t val)
{
	(void)replace_string(&cfg.cd_path, val.str_val);
}

/* Whether directory path should always be resolved to real path (no symbolic
 * links). */
static void
chaselinks_handler(OPT_OP op, optval_t val)
{
	cfg.chase_links = val.bool_val;

	/* Trigger paths update. */
	(void)change_directory(other_view, other_view->curr_dir);
	(void)change_directory(curr_view, curr_view->curr_dir);
	ui_views_update_titles();
}

static void
classify_handler(OPT_OP op, optval_t val)
{
	char type_decs[FT_COUNT][2][9] = {};

	if(str_to_classify(val.str_val, type_decs) == 0)
	{
		int i;

		assert(sizeof(cfg.type_decs) == sizeof(type_decs) && "Arrays diverged");
		memcpy(&cfg.type_decs, &type_decs, sizeof(cfg.type_decs));

		/* Reset cached indexes for name-dependent type_decs. */
		for(i = 0; i < lwin.list_rows; ++i)
		{
			lwin.dir_entry[i].name_dec_num = -1;
		}
		for(i = 0; i < rwin.list_rows; ++i)
		{
			rwin.dir_entry[i].name_dec_num = -1;
		}

		/* 'classify' option affects columns layout, hence views must be reloaded as
		 * loading list of files performs calculation of filename properties. */
		ui_view_schedule_reload(curr_view);
		ui_view_schedule_reload(other_view);
	}
	else
	{
		error = 1;
	}

	init_classify(&val);
	set_option("classify", val, OPT_GLOBAL);
}

/* Fills the decorations array with parsed classification values from the str.
 * It's assumed that decorations array is zeroed.  Returns zero on success,
 * otherwise non-zero is returned. */
static int
str_to_classify(const char str[], char type_decs[FT_COUNT][2][9])
{
	char *saveptr;
	char *str_copy;
	char *token;
	int error_encountered;
	file_dec_t *name_decs = NULL;
	DA_INSTANCE(name_decs);

	str_copy = strdup(str);
	if(str_copy == NULL)
	{
		/* Not enough memory. */
		return 1;
	}

	error_encountered = 0;
	saveptr = NULL;
	for(token = str_copy; (token = split_and_get_dc(token, &saveptr));)
	{
		FileType type;
		const char *expr = NULL;
		const char *suffix = pick_out_decoration(token, &type, &expr);

		if(suffix == NULL)
		{
			vle_tb_append_linef(vle_err, "Invalid filetype: %s", token);
			error_encountered = 1;
		}
		else if(expr != NULL)
		{
			char *error;
			matchers_t *const ms = matchers_alloc(expr, 0, 1, "", &error);
			if(ms == NULL)
			{
				vle_tb_append_linef(vle_err, "Wrong pattern (%s): %s", expr, error);
				free(error);
				error_encountered = 1;
			}
			else
			{
				file_dec_t *const name_dec = DA_EXTEND(name_decs);
				if(name_dec == NULL)
				{
					vle_tb_append_line(vle_err, "Not enough memory");
					error_encountered = 1;
				}
				else
				{
					name_dec->matchers = ms;
				}

				error_encountered |= validate_decorations(token, suffix);

				if(!error_encountered)
				{
					DA_COMMIT(name_decs);
					strcpy(name_dec->prefix, token);
					strcpy(name_dec->suffix, suffix);
				}
				else
				{
					matchers_free(ms);
				}
			}
		}
		else
		{
			error_encountered |= validate_decorations(token, suffix);

			if(!error_encountered)
			{
				strcpy(type_decs[type][DECORATION_PREFIX], token);
				strcpy(type_decs[type][DECORATION_SUFFIX], suffix);
			}
		}
	}
	free(str_copy);

	if(error_encountered)
	{
		free_file_decs(name_decs, DA_SIZE(name_decs));
	}
	else
	{
		free_file_decs(cfg.name_decs, cfg.name_dec_count);
		cfg.name_decs = name_decs;
		cfg.name_dec_count = DA_SIZE(name_decs);
	}

	return error_encountered;
}

/* Puts '\0' after prefix end and returns pointer to the suffix beginning or
 * NULL on unknown filetype specifier.  Sets *expr if this is a name-dependent
 * specification. */
static const char *
pick_out_decoration(char classify_item[], FileType *type, const char **expr)
{
	char *left, *right;

	int filetype;
	for(filetype = 0; filetype < FT_COUNT; ++filetype)
	{
		char name[16];
		char *item_name;
		(void)snprintf(name, sizeof(name), ":%s:", get_type_str(filetype));
		if((item_name = strstr(classify_item, name)) != NULL)
		{
			*type = filetype;
			item_name[0] = '\0';
			return &item_name[strlen(name)];
		}
	}

	left = strstr(classify_item, "::");
	right = (left == NULL) ? NULL : strstr(left + 2, "::");
	if(right != NULL)
	{
		*left = '\0';
		*right = '\0';
		*expr = left + 2;
		return right + 2;
	}

	return NULL;
}

/* Checks whether decorations are well-formatted and prints errors if they
 * aren't.  Returns zero if everything is OK, otherwise non-zero is returned. */
static int
validate_decorations(const char prefix[], const char suffix[])
{
	int error = 0;
	if(strlen(prefix) > 8)
	{
		vle_tb_append_linef(vle_err, "Too long prefix: %s", prefix);
		error = 1;
	}
	if(strlen(suffix) > 8)
	{
		vle_tb_append_linef(vle_err, "Too long suffix: %s", suffix);
		error = 1;
	}
	return error;
}

/* Frees array of file declarations of length count (both elements and array
 * itself). */
static void
free_file_decs(file_dec_t *name_decs, int count)
{
	int i;
	for(i = 0; i < count; ++i)
	{
		matchers_free(name_decs[i].matchers);
	}
	free(name_decs);
}

/* Handles updates of the global 'columns' option, which reflects width of
 * terminal. */
static void
columns_handler(OPT_OP op, optval_t val)
{
	/* Handle case when 'columns' value wasn't yet initialized. */
	if(val.int_val == INT_MIN)
	{
		val.int_val = curr_stats.initial_columns;
		if(val.int_val == INT_MIN)
		{
			val.int_val = getmaxx(stdscr);
		}
	}

	if(val.int_val < MIN_TERM_WIDTH)
	{
		val.int_val = MIN_TERM_WIDTH;
		vle_tb_append_linef(vle_err, "At least %d columns needed", MIN_TERM_WIDTH);
		error = 1;
	}

	if(cfg.columns != val.int_val)
	{
		resize_term(getmaxy(stdscr), val.int_val);
		update_screen(UT_REDRAW);
		cfg.columns = getmaxx(stdscr);
	}

	/* Need to update value of option in case it was corrected above. */
	val.int_val = cfg.columns;
	set_option("columns", val, OPT_GLOBAL);
}

static void
confirm_handler(OPT_OP op, optval_t val)
{
	cfg.confirm = val.set_items;
}

/* Parses set of compatibility flags and changes configuration accordingly. */
static void
cpoptions_handler(OPT_OP op, optval_t val)
{
	const char *p;

	/* Set all options to new kind of behaviour. */
	cfg.filter_inverted_by_default = 0;
	cfg.selection_is_primary = 0;
	cfg.tab_switches_pane = 0;

	/* Reset behaviour to compatibility mode for each flag listed in the value. */
	p = val.str_val;
	while(*p != '\0')
	{
		switch(*p)
		{
			case 'f':
				cfg.filter_inverted_by_default = 1;
				break;
			case 's':
				cfg.selection_is_primary = 1;
				break;
			case 't':
				cfg.tab_switches_pane = 1;
				break;

			default:
				assert(0 && "Unhandled cpoptions flag.");
				break;
		}
		++p;
	}
}

/* Assigns new value for 'cvoptions'. */
static void
cvoptions_handler(OPT_OP op, optval_t val)
{
	cfg.cvoptions = val.set_items;
}

/* Handles updates of the 'deleteprg' option. */
static void
deleteprg_handler(OPT_OP op, optval_t val)
{
	replace_string(&cfg.delete_prg, val.str_val);
}

/* Handles changes to the way directory size is displayed in a view. */
static void
dirsize_handler(OPT_OP op, optval_t val)
{
	cfg.view_dir_size = val.enum_item;
	update_screen(UT_REDRAW);
}

static void
dotdirs_handler(OPT_OP op, optval_t val)
{
	cfg.dot_dirs = val.set_items;
	update_screen(UT_FULL);
}

static void
fastrun_handler(OPT_OP op, optval_t val)
{
	cfg.fast_run = val.bool_val;
}

/* Handles new value for 'fillchars' option. */
static void
fillchars_handler(OPT_OP op, optval_t val)
{
	char *new_val = strdup(val.str_val);
	char *part = new_val, *state = NULL;

	/* XXX: we shouldn't reset this value unless format is correct. */
	(void)replace_string(&cfg.border_filler, " ");

	while((part = split_and_get(part, ',', &state)) != NULL)
	{
		if(starts_with_lit(part, "vborder:"))
		{
			(void)replace_string(&cfg.border_filler, after_first(part, ':'));
		}
		else
		{
			break_at(part, ':');
			vle_tb_append_linef(vle_err, "Unknown key for 'fillchars' option: %s",
					part);
			break;
		}
	}
	free(new_val);

	if(part != NULL)
	{
		reset_fillchars();
	}

	curr_stats.need_update = UT_REDRAW;
}

/* Resets value of 'fillchars' option by composing it from current
 * configuration. */
static void
reset_fillchars(void)
{
	optval_t val;
	char value[128];

	value[0] = '\0';

	if(strcmp(cfg.border_filler, " ") != 0)
	{
		snprintf(value, sizeof(value), "vborder:%s", cfg.border_filler);
	}

	val.str_val = value;
	set_option("fillchars", val, OPT_GLOBAL);
}

static void
findprg_handler(OPT_OP op, optval_t val)
{
	(void)replace_string(&cfg.find_prg, val.str_val);
}

static void
followlinks_handler(OPT_OP op, optval_t val)
{
	cfg.follow_links = val.bool_val;
}

static void
fusehome_handler(OPT_OP op, optval_t val)
{
	char *const expanded_path = expand_path(val.str_val);
	/* Set cfg.fuse_home in the correct way. */
	if(cfg_set_fuse_home(expanded_path) != 0)
	{
		/* Reset the 'fusehome' options to its previous value. */
		val.str_val = cfg.fuse_home;
		set_option("fusehome", val, OPT_GLOBAL);
	}
	free(expanded_path);
}

static void
gdefault_handler(OPT_OP op, optval_t val)
{
	cfg.gdefault = val.bool_val;
}

static void
grepprg_handler(OPT_OP op, optval_t val)
{
	(void)replace_string(&cfg.grep_prg, val.str_val);
}

static void
history_handler(OPT_OP op, optval_t val)
{
	cfg_resize_histories(val.int_val);
}

static void
hlsearch_handler(OPT_OP op, optval_t val)
{
	cfg.hl_search = val.bool_val;

	/* Reset remembered search results, so that n/N trigger search repetition and
	 * selection/unselection of elements. */
	reset_search_results(curr_view);
	reset_search_results(other_view);
}

static void
iec_handler(OPT_OP op, optval_t val)
{
	cfg.sizefmt.ieci_prefixes = val.bool_val;

	redraw_lists();
}

static void
ignorecase_handler(OPT_OP op, optval_t val)
{
	cfg.ignore_case = val.bool_val;
}

static void
incsearch_handler(OPT_OP op, optval_t val)
{
	cfg.inc_search = val.bool_val;
}

/* Handles changes of 'iooptions'.  Updates related configuration values. */
static void
iooptions_handler(OPT_OP op, optval_t val)
{
	cfg.fast_file_cloning = ((val.set_items & 1) != 0);
}

static void
laststatus_handler(OPT_OP op, optval_t val)
{
	cfg.display_statusline = val.bool_val;
	if(cfg.display_statusline)
	{
		if(curr_stats.split == VSPLIT)
		{
			scroll_line_down(&lwin);
		}
		scroll_line_down(&rwin);
	}

	curr_stats.need_update = UT_REDRAW;
}

/* Handles updates of the global 'lines' option, which reflects height of
 * terminal. */
static void
lines_handler(OPT_OP op, optval_t val)
{
	/* Handle case when 'lines' value wasn't yet initialized. */
	if(val.int_val == INT_MIN)
	{
		val.int_val = curr_stats.initial_lines;
		if(val.int_val == INT_MIN)
		{
			val.int_val = getmaxy(stdscr);
		}
	}

	if(val.int_val < MIN_TERM_HEIGHT)
	{
		val.int_val = MIN_TERM_HEIGHT;
		vle_tb_append_linef(vle_err, "At least %d lines needed", MIN_TERM_HEIGHT);
		error = 1;
	}

	if(cfg.lines != val.int_val)
	{
		LOG_INFO_MSG("resize_term(%d, %d)", val.int_val, getmaxx(stdscr));
		resize_term(val.int_val, getmaxx(stdscr));
		update_screen(UT_REDRAW);
		cfg.lines = getmaxy(stdscr);
	}

	/* Need to update value of option in case it was corrected above. */
	val.int_val = cfg.lines;
	set_option("lines", val, OPT_GLOBAL);
}

static void
locateprg_handler(OPT_OP op, optval_t val)
{
	(void)replace_string(&cfg.locate_prg, val.str_val);
}

/* Minimum period on waiting for the input.  Works together with timeoutlen. */
static void
mintimeoutlen_handler(OPT_OP op, optval_t val)
{
	if(val.int_val <= 0)
	{
		vle_tb_append_linef(vle_err, "Argument must be > 0: %d", val.int_val);
		error = 1;
		val.int_val = 1;
		set_option("mintimeoutlen", val, OPT_GLOBAL);
		return;
	}

	cfg.min_timeout_len = val.int_val;
}

static void
scroll_line_down(FileView *view)
{
	view->window_rows--;
	if(view->list_pos == view->top_line + view->window_rows + 1)
	{
		view->top_line++;
		view->curr_line--;
		draw_dir_list(view);
	}
	wresize(view->win, view->window_rows + 1, view->window_width + 1);
}

static void
rulerformat_handler(OPT_OP op, optval_t val)
{
	(void)replace_string(&cfg.ruler_format, val.str_val);
}

static void
runexec_handler(OPT_OP op, optval_t val)
{
	cfg.auto_execute = val.bool_val;
}

static void
scrollbind_handler(OPT_OP op, optval_t val)
{
	cfg.scroll_bind = val.bool_val;
	update_scroll_bind_offset();
}

static void
scrolloff_handler(OPT_OP op, optval_t val)
{
	if(val.int_val < 0)
	{
		vle_tb_append_linef(vle_err, "Invalid scroll size: %d", val.int_val);
		error = 1;
		reset_option_to_default("scrolloff", OPT_GLOBAL);
		return;
	}

	cfg.scroll_off = val.int_val;
	if(cfg.scroll_off > 0)
	{
		fview_cursor_redraw(curr_view);
	}
}

static void
shell_handler(OPT_OP op, optval_t val)
{
	cfg_set_shell(val.str_val);
}

/* Handles changes of 'shortmess' option by expanding flags into actual option
 * values. */
static void
shortmess_handler(OPT_OP op, optval_t val)
{
	const char *p;

	cfg.trunc_normal_sb_msgs = 0;
	cfg.shorten_title_paths = 0;

	p = val.str_val;
	while(*p != '\0')
	{
		if(*p == 'T')
		{
			cfg.trunc_normal_sb_msgs = 1;
		}
		else if(*p == 'p')
		{
			cfg.shorten_title_paths = 1;
		}
		++p;
	}

	ui_views_update_titles();
}

#ifndef _WIN32
static void
slowfs_handler(OPT_OP op, optval_t val)
{
	(void)replace_string(&cfg.slow_fs_list, val.str_val);
}
#endif

static void
smartcase_handler(OPT_OP op, optval_t val)
{
	cfg.smart_case = val.bool_val;
}

static void
sortnumbers_handler(OPT_OP op, optval_t val)
{
	cfg.sort_numbers = val.bool_val;
	resort_dir_list(1, curr_view);
	resort_dir_list(1, other_view);
	redraw_lists();
}

/* Handles switch that controls visibility of dot files globally. */
static void
dotfiles_global(OPT_OP op, optval_t val)
{
	curr_view->hide_dot_g = !val.bool_val;
}

/* Handles switch that controls visibility of dot files locally. */
static void
dotfiles_local(OPT_OP op, optval_t val)
{
	curr_view->hide_dot = !val.bool_val;
	ui_view_schedule_reload(curr_view);
}

/* Handles switch that controls column vs. ls-like view in global option. */
static void
lsview_global(OPT_OP op, optval_t val)
{
	curr_view->ls_view_g = val.bool_val;
}

/* Handles switch that controls column vs. ls-like view in local option. */
static void
lsview_local(OPT_OP op, optval_t val)
{
	fview_set_lsview(curr_view, val.bool_val);
}

/* Handles file numbers displaying toggle in global option. */
static void
number_global(OPT_OP op, optval_t val)
{
	update_num_type(curr_view, &curr_view->num_type_g, NT_SEQ, val.bool_val);
}

/* Handles file numbers displaying toggle in local option. */
static void
number_local(OPT_OP op, optval_t val)
{
	update_num_type(curr_view, &curr_view->num_type, NT_SEQ, val.bool_val);
}

/* Handles changes of minimum width of file number field in global option. */
static void
numberwidth_global(OPT_OP op, optval_t val)
{
	set_numberwidth(curr_view, &curr_view->num_width_g, val.int_val);
}

/* Handles changes of minimum width of file number field in local option. */
static void
numberwidth_local(OPT_OP op, optval_t val)
{
	set_numberwidth(curr_view, &curr_view->num_width, val.int_val);
}

/* Sets number width for the view. */
static void
set_numberwidth(FileView *view, int *num_width, int width)
{
	*num_width = width;

	if(ui_view_displays_numbers(view))
	{
		redraw_view(view);
	}
}

/* Handles relative file numbers displaying toggle in global option. */
static void
relativenumber_global(OPT_OP op, optval_t val)
{
	update_num_type(curr_view, &curr_view->num_type_g, NT_REL, val.bool_val);
}

/* Handles relative file numbers displaying toggle in local option. */
static void
relativenumber_local(OPT_OP op, optval_t val)
{
	update_num_type(curr_view, &curr_view->num_type, NT_REL, val.bool_val);
}

/* Handles toggling of boolean number related option and updates current view if
 * needed. */
static void
update_num_type(FileView *view, NumberingType *num_type, NumberingType type,
		int enable)
{
	const NumberingType old_num_type = *num_type;

	*num_type = enable
	          ? (old_num_type | type)
	          : (old_num_type & ~type);

	if(*num_type != old_num_type)
	{
		redraw_view(view);
	}
}

/* Handler for global 'sort' option, parses the value and checks it for
 * correctness. */
static void
sort_global(OPT_OP op, optval_t val)
{
	set_sort(curr_view, curr_view->sort_g, val.str_val);
}

/* Handler for local 'sort' option, parses the value and checks it for
 * correctness. */
static void
sort_local(OPT_OP op, optval_t val)
{
	/* Make sure we don't sort unsorted custom view on :restart or when it's a
	 * compare view. */
	char *const sort = (curr_stats.restart_in_progress ||
	                    cv_compare(curr_view->custom.type))
	                 ? ui_view_sort_list_get(curr_view, curr_view->sort)
	                 : curr_view->sort;
	set_sort(curr_view, sort, val.str_val);
}

/* Sets sorting value for the view. */
static void
set_sort(FileView *view, char sort_keys[], char order[])
{
	char *part = order, *state = NULL;
	int key_count = 0;

	while((part = split_and_get(part, ',', &state)) != NULL)
	{
		char *name = part;
		int reverse = 0;
		int pos;
		int key_pos;

		if(*name == '-' || *name == '+')
		{
			reverse = (*name == '-');
			++name;
		}

		pos = string_array_pos((char **)&sort_enum[1], ARRAY_LEN(sort_enum) - 1,
				name);
		if(pos == -1)
		{
			if(name[0] != '\0')
			{
				vle_tb_append_linef(vle_err, "Skipped unknown 'sort' value: %s", name);
				error = 1;
			}
			continue;
		}
		++pos;

		for(key_pos = 0; key_pos < key_count; ++key_pos)
		{
			if(abs(sort_keys[key_pos]) == pos)
			{
				break;
			}
		}

		sort_keys[key_pos] = reverse ? -pos : pos;

		if(key_pos == key_count)
		{
			++key_count;
		}
	}

	if(key_count < SK_COUNT)
	{
		sort_keys[key_count] = SK_NONE;
	}
	ui_view_sort_list_ensure_well_formed(view, sort_keys);

	if(sort_keys == view->sort)
	{
		sorting_changed(view, 0);
	}

	load_sort_option_inner(view, sort_keys);
}

/* Handles global 'sortgroup' option changes. */
static void
sortgroups_global(OPT_OP op, optval_t val)
{
	set_sortgroups(curr_view, &curr_view->sort_groups_g, val.str_val);
}

/* Handles local 'sortgroup' option changes. */
static void
sortgroups_local(OPT_OP op, optval_t val)
{
	set_sortgroups(curr_view, &curr_view->sort_groups, val.str_val);
	sorting_changed(curr_view, 0);
}

/* Sets sort_groups fields (*opt) of the view to the value handling malformed
 * values correctly. */
static void
set_sortgroups(FileView *view, char **opt, char value[])
{
	OPT_SCOPE scope = (opt == &view->sort_groups) ? OPT_LOCAL : OPT_GLOBAL;
	int failure = 0;

	char *first = NULL;
	char *group = value, *state = NULL;
	while((group = split_and_get(group, ',', &state)) != NULL)
	{
		regex_t regex;
		const int err = regcomp(&regex, group, REG_EXTENDED | REG_ICASE);
		if(err != 0)
		{
			vle_tb_append_linef(vle_err, "Regexp error in %s: %s", group,
					get_regexp_error(err, &regex));
			failure = 1;
		}
		regfree(&regex);

		if(first == NULL)
		{
			first = strdup(group);
		}
	}

	if(failure)
	{
		optval_t val = { .str_val = *opt };

		free(first);
		error = 1;
		set_option("viewcolumns", val, scope);
		return;
	}

	if(first != NULL)
	{
		if(scope == OPT_LOCAL)
		{
			regfree(&view->primary_group);
			(void)regcomp(&view->primary_group, first, REG_EXTENDED | REG_ICASE);
		}
		free(first);
	}

	replace_string(opt, value);
}

/* Reacts on changes of view sorting. */
static void
sorting_changed(FileView *view, int defer_slow)
{
	/* Reset search results, which might be outdated after resorting. */
	view->matches = 0;
	fview_sorting_updated(view);
	if(!defer_slow)
	{
		resort_view(view);
	}
}

/* Handles global 'sortorder' option and corrects ordering for primary sorting
 * key. */
static void
sortorder_global(OPT_OP op, optval_t val)
{
	set_sortorder(curr_view, (val.enum_item == 1) ? 0 : 1, curr_view->sort_g);
}

/* Handles local 'sortorder' option and corrects ordering for primary sorting
 * key. */
static void
sortorder_local(OPT_OP op, optval_t val)
{
	set_sortorder(curr_view, (val.enum_item == 1) ? 0 : 1, curr_view->sort);
}

/* Updates sorting order for the view. */
static void
set_sortorder(FileView *view, int ascending, char sort_keys[])
{
	if((ascending ? +1 : -1)*sort_keys[0] < 0)
	{
		sort_keys[0] = -sort_keys[0];

		if(sort_keys == view->sort)
		{
			resort_view(view);
			load_sort_option(view);
		}
	}
}

/* Handler of global 'viewcolumns' option, which defines custom view columns. */
static void
viewcolumns_global(OPT_OP op, optval_t val)
{
	replace_string(&curr_view->view_columns_g, val.str_val);
}

/* Handler of local 'viewcolumns' option, which defines custom view columns. */
static void
viewcolumns_local(OPT_OP op, optval_t val)
{
	set_viewcolumns(curr_view, val.str_val);
}

/* Setups view columns for the view. */
static void
set_viewcolumns(FileView *view, const char view_columns[])
{
	const int update_columns_ui = ui_view_displays_columns(view);
	set_view_columns_option(view, view_columns, update_columns_ui);
}

void
load_dot_filter_option(const FileView *view)
{
	const optval_t val = { .bool_val = !view->hide_dot };
	set_option("dotfiles", val, OPT_GLOBAL);
	set_option("dotfiles", val, OPT_LOCAL);
}

void
load_view_columns_option(FileView *view, const char value[])
{
	set_view_columns_option(view, value, 1);
}

/* Updates view columns value as if 'viewcolumns' option has been changed.
 * Doesn't change actual value of the option, which is important for setting
 * sorting order via sort dialog. */
static void
set_view_columns_option(FileView *view, const char value[], int update_ui)
{
	const char *new_value = (value[0] == '\0') ? DEFAULT_VIEW_COLUMNS : value;
	columns_t *columns = update_ui ? view->columns : NULL;

	if(update_ui)
	{
		columns_clear(columns);
	}

	if(parse_columns(columns, &add_column, &map_name, new_value, view) != 0)
	{
		optval_t val;

		vle_tb_append_line(vle_err, "Invalid format of 'viewcolumns' option");
		error = 1;

		if(update_ui)
		{
			const char *old_value = (view->view_columns[0] == '\0')
			                      ? DEFAULT_VIEW_COLUMNS
			                      : view->view_columns;
			(void)parse_columns(columns, &add_column, &map_name, old_value, view);
		}

		val.str_val = view->view_columns;
		set_option("viewcolumns", val, OPT_LOCAL);
	}
	else
	{
		/* Set value specified by user.  Can't use DEFAULT_VIEW_COLUMNS here,
		 * as we need empty value of view->view_columns to signal about disabled
		 * columns customization. */
		(void)replace_string(&view->view_columns, value);
		if(update_ui)
		{
			ui_view_schedule_redraw(view);
		}
	}
}

/* Adds new column to view columns. */
static void
add_column(columns_t *columns, column_info_t column_info)
{
	/* Handle dry run mode, when we don't actually update column view. */
	if(columns != NULL)
	{
		columns_add_column(columns, column_info);
	}
}

/* Maps column name to column id.  Returns column id. */
static int
map_name(const char name[], void *arg)
{
	int pos;

	/* Handle secondary key (designated by {}). */
	if(*name == '\0')
	{
		const FileView *const view = arg;
		const char *const sort = ui_view_sort_list_get(view, view->sort);
		return (int)get_secondary_key((SortingKey)abs(sort[0]));
	}

	pos = string_array_pos((char **)&sort_enum[1], ARRAY_LEN(sort_enum) - 1,
			name);
	return (pos >= 0) ? (pos + 1) : -1;
}

void
load_geometry(void)
{
	optval_t val;
	val.int_val = cfg.columns;
	set_option("columns", val, OPT_GLOBAL);
	val.int_val = cfg.lines;
	set_option("lines", val, OPT_GLOBAL);
}

static void
resort_view(FileView * view)
{
	resort_dir_list(1, view);
	ui_view_schedule_redraw(curr_view);
}

static void
statusline_handler(OPT_OP op, optval_t val)
{
	(void)replace_string(&cfg.status_line, val.str_val);
}

/* Sets when to display key suggestions. */
static void
suggestoptions_handler(OPT_OP op, optval_t val)
{
	int new_value = 0;
	int maxregfiles = 5;
	int delay = 500;

	char *new_val = strdup(val.str_val);
	char *part = new_val, *state = NULL;

	while((part = split_and_get(part, ',', &state)) != NULL)
	{
		if(strcmp(part, "normal") == 0)         new_value |= SF_NORMAL;
		else if(strcmp(part, "visual") == 0)    new_value |= SF_VISUAL;
		else if(strcmp(part, "view") == 0)      new_value |= SF_VIEW;
		else if(strcmp(part, "otherpane") == 0) new_value |= SF_OTHERPANE;
		else if(strcmp(part, "keys") == 0)      new_value |= SF_KEYS;
		else if(strcmp(part, "marks") == 0)     new_value |= SF_MARKS;
		else if(starts_with_lit(part, "delay:"))
		{
			const char *const num = after_first(part, ':');
			new_value |= SF_DELAY;
			if(!read_int(after_first(part, ':'), &delay))
			{
				vle_tb_append_linef(vle_err, "Failed to parse \"delay\" value: %s",
						num);
				break;
			}
			if(delay < 0)
			{
				vle_tb_append_line(vle_err, "Delay can't be negative");
				break;
			}
		}
		else if(starts_with_lit(part, "registers:"))
		{
			const char *const num = after_first(part, ':');
			new_value |= SF_REGISTERS;
			if(!read_int(after_first(part, ':'), &maxregfiles))
			{
				vle_tb_append_linef(vle_err, "Failed to parse \"registers\" value: %s",
						num);
				break;
			}
			if(maxregfiles <= 0)
			{
				vle_tb_append_line(vle_err,
						"Must be at least one displayed register file");
				break;
			}
		}
		else if(strcmp(part, "delay") == 0)     new_value |= SF_DELAY;
		else if(strcmp(part, "registers") == 0) new_value |= SF_REGISTERS;
		else
		{
			break_at(part, ':');
			vle_tb_append_linef(vle_err,
					"Invalid key for 'suggestoptions' option: %s", part);
			break;
		}
	}
	free(new_val);

	if(part == NULL)
	{
		cfg.sug.flags = new_value;
		cfg.sug.maxregfiles = maxregfiles;
		cfg.sug.delay = delay;
	}
	else
	{
		reset_suggestoptions();
	}
}

/* Converts line to a number.  Handles overflow/underflow.  Returns non-zero on
 * success and zero otherwise. */
static int
read_int(const char line[], int *i)
{
	char *endptr;
	const long l = strtol(line, &endptr, 10);

	*i = (l > INT_MAX) ? INT_MAX : ((l < INT_MIN) ? INT_MIN : l);

	return *line != '\0' && *endptr == '\0';
}

/* Sets value of 'suggestoptions' based on current configuration values. */
static void
reset_suggestoptions(void)
{
	optval_t val;
	vle_textbuf *const descr = vle_tb_create();

	if(cfg.sug.flags & SF_NORMAL)    vle_tb_appendf(descr, "%s", "normal,");
	if(cfg.sug.flags & SF_VISUAL)    vle_tb_appendf(descr, "%s", "visual,");
	if(cfg.sug.flags & SF_VIEW)      vle_tb_appendf(descr, "%s", "view,");
	if(cfg.sug.flags & SF_OTHERPANE) vle_tb_appendf(descr, "%s", "otherpane,");
	if(cfg.sug.flags & SF_KEYS)      vle_tb_appendf(descr, "%s", "keys,");
	if(cfg.sug.flags & SF_MARKS)     vle_tb_appendf(descr, "%s", "marks,");
	if(cfg.sug.flags & SF_DELAY)
	{
		if(cfg.sug.delay == 500)
		{
			vle_tb_appendf(descr, "%s", "delay,");
		}
		else
		{
			vle_tb_appendf(descr, "delay:%d,", cfg.sug.delay);
		}
	}
	if(cfg.sug.flags & SF_REGISTERS)
	{
		if(cfg.sug.maxregfiles == 5)
		{
			vle_tb_appendf(descr, "%s", "registers,");
		}
		else
		{
			vle_tb_appendf(descr, "registers:%d,", cfg.sug.maxregfiles);
		}
	}

	val.str_val = (char *)vle_tb_get_data(descr);
	set_option("suggestoptions", val, OPT_GLOBAL);

	vle_tb_free(descr);
}

/* Makes vifm prefer to perform file-system operations with external
 * applications on rather then with system calls.  The option will be eventually
 * removed.  Mostly *nix-like systems are affected. */
static void
syscalls_handler(OPT_OP op, optval_t val)
{
	cfg.use_system_calls = val.bool_val;
}

static void
tabstop_handler(OPT_OP op, optval_t val)
{
	if(val.int_val <= 0)
	{
		vle_tb_append_linef(vle_err, "Argument must be positive: %d", val.int_val);
		error = 1;
		reset_option_to_default("tabstop", OPT_GLOBAL);
		return;
	}

	cfg.tab_stop = val.int_val;
	text_option_changed();
}

static void
timefmt_handler(OPT_OP op, optval_t val)
{
	free(cfg.time_format);
	cfg.time_format = malloc(1 + strlen(val.str_val) + 1);
	strcpy(cfg.time_format, " ");
	strcat(cfg.time_format, val.str_val);

	redraw_lists();
}

/* Maximum period on waiting for the input.  Works together with
 * mintimeoutlen. */
static void
timeoutlen_handler(OPT_OP op, optval_t val)
{
	if(val.int_val < 0)
	{
		vle_tb_append_linef(vle_err, "Argument must be >= 0: %d", val.int_val);
		error = 1;
		val.int_val = 0;
		set_option("timeoutlen", val, OPT_GLOBAL);
		return;
	}

	cfg.timeout_len = val.int_val;
}

/* Whether to update terminal title according to current location or not. */
static void
title_handler(OPT_OP op, optval_t val)
{
	cfg.set_title = val.bool_val;
	if(cfg.set_title)
	{
		ui_view_title_update(curr_view);
	}
	else
	{
		term_title_update(NULL);
	}
}

static void
trash_handler(OPT_OP op, optval_t val)
{
	cfg.use_trash = val.bool_val;
}

static void
trashdir_handler(OPT_OP op, optval_t val)
{
	char *const expanded_path = expand_path(val.str_val);
	if(set_trash_dir(expanded_path) != 0)
	{
		/* Reset the 'trashdir' option to its previous value. */
		val.str_val = cfg.trash_dir;
		set_option("trashdir", val, OPT_GLOBAL);
	}
	free(expanded_path);
}

/* Parses set of TUI flags and changes appearance configuration accordingly. */
static void
tuioptions_handler(OPT_OP op, optval_t val)
{
	const char *p;

	/* Turn all flags off. */
	cfg.extra_padding = 0;
	cfg.side_borders_visible = 0;

	/* And set the ones present in the value. */
	p = val.str_val;
	while(*p != '\0')
	{
		switch(*p)
		{
			case 'p':
				cfg.extra_padding = 1;
				break;
			case 's':
				cfg.side_borders_visible = 1;
				break;

			default:
				assert(0 && "Unhandled tuioptions flag.");
				break;
		}
		++p;
	}

	curr_stats.need_update = UT_REDRAW;
}

static void
undolevels_handler(OPT_OP op, optval_t val)
{
	cfg.undo_levels = val.int_val;
}

static void
vicmd_handler(OPT_OP op, optval_t val)
{
	(void)replace_string(&cfg.vi_command, val.str_val);
	cfg.vi_cmd_bg = cut_suffix(cfg.vi_command, "&");
}

static void
vixcmd_handler(OPT_OP op, optval_t val)
{
	(void)replace_string(&cfg.vi_x_command, val.str_val);
	cfg.vi_x_cmd_bg = cut_suffix(cfg.vi_x_command, "&");
}

static void
vifminfo_handler(OPT_OP op, optval_t val)
{
	cfg.vifm_info = val.set_items;
}

static void
vimhelp_handler(OPT_OP op, optval_t val)
{
	cfg.use_vim_help = val.bool_val;
}

static void
wildmenu_handler(OPT_OP op, optval_t val)
{
	cfg.wild_menu = val.bool_val;
}

/* Wild menu style, which defines how it's presented. */
static void
wildstyle_handler(OPT_OP op, optval_t val)
{
	/* There are only two possible values. */
	cfg.wild_popup = (val.enum_item == 1);
}

/* Handles setting value of 'wordchars' by parsing list of ranges into character
 * array. */
static void
wordchars_handler(OPT_OP op, optval_t val)
{
	char word_chars[256] = { };

	char *new_val = strdup(val.str_val);
	char *part = new_val, *state = NULL;
	while((part = split_and_get(part, ',', &state)) != NULL)
	{
		int from, to;
		if(parse_range(part, &from, &to) != 0)
		{
			error = 1;
			break;
		}

		while(from <= to)
		{
			word_chars[from++] = 1;
		}
	}
	free(new_val);

	if(part == NULL)
	{
		memcpy(&cfg.word_chars, &word_chars, sizeof(cfg.word_chars));
	}
}

/* Parses range, which can be shortened to single endpoint if first element
 * matches last one.  Returns non-zero on error, otherwise zero is returned. */
static int
parse_range(const char range[], int *from, int *to)
{
	const char *p = range;

	if(parse_endpoint(&p, from) != 0)
	{
		vle_tb_append_linef(vle_err, "Wrong range: %s", range);
		return 1;
	}
	if(*p == '-')
	{
		++p;
		if(parse_endpoint(&p, to) != 0)
		{
			vle_tb_append_linef(vle_err, "Wrong range: %s", range);
			return 1;
		}
	}
	else
	{
		*to = *from;
	}

	if(*p != '\0')
	{
		vle_tb_append_linef(vle_err, "Wrong range: %s", range);
		return 1;
	}

	if(*from > *to)
	{
		vle_tb_append_linef(vle_err, "Inversed range: %s", range);
		return 1;
	}

	return 0;
}

/* Parses single endpoint of a range.  It's either a number or a character.
 * Returns non-zero on error, otherwise zero is returned. */
static int
parse_endpoint(const char **str, int *endpoint)
{
	if(**str == '\0')
	{
		vle_tb_append_line(vle_err, "Range bound can't be empty");
		return 1;
	}

	if(isdigit(**str))
	{
		char *endptr;
		const long int val = strtol(*str, &endptr, 10);
		if(val < 0 || val >= 256)
		{
			vle_tb_append_linef(vle_err, "Wrong value: %ld", val);
			return 1;
		}
		*str = endptr;
		*endpoint = val;
	}
	else
	{
		*endpoint = (unsigned char)**str;
		++*str;
	}
	return 0;
}

static void
wrap_handler(OPT_OP op, optval_t val)
{
	cfg.wrap_quick_view = val.bool_val;
	text_option_changed();
}

/* Updates preview pane and explore mode.  Should be called when any of options
 * that affect those modes is changed. */
static void
text_option_changed(void)
{
	if(curr_stats.view)
	{
		qv_draw(curr_view);
	}
	else if(other_view->explore_mode)
	{
		view_redraw();
	}
}

static void
wrapscan_handler(OPT_OP op, optval_t val)
{
	cfg.wrap_scan = val.bool_val;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
