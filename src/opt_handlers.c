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
#include "modes/view.h"
#include "ui/column_view.h"
#include "ui/fileview.h"
#include "ui/quickview.h"
#include "ui/statusbar.h"
#include "ui/ui.h"
#include "utils/log.h"
#include "utils/macros.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/utils.h"
#include "filelist.h"
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

static void init_classify(optval_t *val);
static void init_cpoptions(optval_t *val);
static const char * to_endpoint(int i, char buffer[]);
static void init_timefmt(optval_t *val);
static void init_trash_dir(optval_t *val);
static void init_lsview(optval_t *val);
static void init_shortmess(optval_t *val);
static void init_number(optval_t *val);
static void init_numberwidth(optval_t *val);
static void init_relativenumber(optval_t *val);
static void init_sort(optval_t *val);
static void init_sortorder(optval_t *val);
static void init_tuioptions(optval_t *val);
static void init_viewcolumns(optval_t *val);
static void init_wordchars(optval_t *val);
static void load_options_defaults(void);
static void add_options(void);
static void load_sort_option_inner(FileView *view, char sort_keys[]);
static void aproposprg_handler(OPT_OP op, optval_t val);
static void autochpos_handler(OPT_OP op, optval_t val);
static void cdpath_handler(OPT_OP op, optval_t val);
static void chaselinks_handler(OPT_OP op, optval_t val);
static void classify_handler(OPT_OP op, optval_t val);
static int str_to_classify(const char str[], char decorations[FT_COUNT][2]);
static const char * pick_out_decoration(const char classify_item[],
		FileType *type);
static void columns_handler(OPT_OP op, optval_t val);
static void confirm_handler(OPT_OP op, optval_t val);
static void cpoptions_handler(OPT_OP op, optval_t val);
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
static int parse_range(const char range[], int *from, int *to);
static int parse_endpoint(const char **str, int *endpoint);
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
static void sortorder_global(OPT_OP op, optval_t val);
static void sortorder_local(OPT_OP op, optval_t val);
static void set_sortorder(FileView *view, int ascending, char sort_keys[]);
static void viewcolumns_global(OPT_OP op, optval_t val);
static void viewcolumns_local(OPT_OP op, optval_t val);
static void set_viewcolumns(FileView *view, const char view_columns[]);
static void set_view_columns_option(FileView *view, const char value[],
		int update_ui);
static void add_column(columns_t columns, column_info_t column_info);
static int map_name(const char name[], void *arg);
static void resort_view(FileView * view);
static void statusline_handler(OPT_OP op, optval_t val);
static void syscalls_handler(OPT_OP op, optval_t val);
static void tabstop_handler(OPT_OP op, optval_t val);
static void timefmt_handler(OPT_OP op, optval_t val);
static void timeoutlen_handler(OPT_OP op, optval_t val);
static void trash_handler(OPT_OP op, optval_t val);
static void trashdir_handler(OPT_OP op, optval_t val);
static void tuioptions_handler(OPT_OP op, optval_t val);
static void undolevels_handler(OPT_OP op, optval_t val);
static void vicmd_handler(OPT_OP op, optval_t val);
static void vixcmd_handler(OPT_OP op, optval_t val);
static void vifminfo_handler(OPT_OP op, optval_t val);
static void vimhelp_handler(OPT_OP op, optval_t val);
static void wildmenu_handler(OPT_OP op, optval_t val);
static void wordchars_handler(OPT_OP op, optval_t val);
static void wrap_handler(OPT_OP op, optval_t val);
static void text_option_changed(void);
static void wrapscan_handler(OPT_OP op, optval_t val);

static const char * sort_enum[] = {
	"ext",
	"name",
#ifndef _WIN32
	"gid",
	"gname",
	"mode",
	"uid",
	"uname",
#endif
	"size",
	"atime",
	"ctime",
	"mtime",
	"iname",
#ifndef _WIN32
	"perms",
#endif
	"dir",
	"type",
	"fileext",
};
ARRAY_GUARD(sort_enum, SK_COUNT);

static const char cpoptions_list[] = "fst";
static const char * cpoptions_vals = cpoptions_list;
#define cpoptions_count (ARRAY_LEN(cpoptions_list) - 1)

static const char * dotdirs_vals[] = {
	"rootparent",
	"nonrootparent",
};
ARRAY_GUARD(dotdirs_vals, NUM_DOT_DIRS);

/* Possible flags of 'shortmess' and their count. */
static const char shortmess_list[] = "Tp";
static const char *shortmess_vals = shortmess_list;
#define shortmess_count (ARRAY_LEN(shortmess_list) - 1)

/* Possible flags of 'tuioptions' and their count. */
static const char tuioptions_list[] = "ps";
static const char *tuioptions_vals = tuioptions_list;
#define tuioptions_count (ARRAY_LEN(tuioptions_list) - 1)

/* Possible keys of 'fillchars' option. */
static const char *fillchars_enum[] = {
	"vborder:",
};

/* Possible values of 'sort' option. */
static const char *sort_types[] = {
	"ext",   "+ext",   "-ext",
	"name",  "+name",  "-name",
#ifndef _WIN32
	"gid",   "+gid",   "-gid",
	"gname", "+gname", "-gname",
	"mode",  "+mode",  "-mode",
	"uid",   "+uid",   "-uid",
	"uname", "+uname", "-uname",
#endif
	"size",  "+size",  "-size",
	"atime", "+atime", "-atime",
	"ctime", "+ctime", "-ctime",
	"mtime", "+mtime", "-mtime",
	"iname", "+iname", "-iname",
#ifndef _WIN32
	"perms", "+perms", "-perms",
#endif
	"dir",     "+dir",     "-dir",
	"type",    "+type",    "-type",
	"fileext", "+fileext", "-fileext",
};
ARRAY_GUARD(sort_types, SK_COUNT*3);

/* Possible values of 'sortorder' option. */
static const char *sortorder_enum[] = {
	"ascending",
	"descending",
};

/* Possible values of 'vifminfo' option. */
static const char *vifminfo_set[] = {
	"options",
	"filetypes",
	"commands",
	"bookmarks",
	"bmarks",
	"tui",
	"dhistory",
	"state",
	"cs",
	"savedirs",
	"chistory",
	"shistory",
	"dirstack",
	"registers",
	"phistory",
	"fhistory",
};

/* Empty value to satisfy default initializer. */
static char empty_val[] = "";
static char *empty = empty_val;

static struct
{
	const char *name;
	const char *abbr;
	OPT_TYPE type;
	int val_count;
	const char **vals;
	opt_handler global_handler;
	opt_handler local_handler;
	optinit_t initializer;
	optval_t val;
}
options[] = {
	/* Global options. */
	{ "aproposprg", "",
	  OPT_STR, 0, NULL, &aproposprg_handler, NULL,
	  { .ref.str_val = &cfg.apropos_prg },
	},
	{ "autochpos", "",
	  OPT_BOOL, 0, NULL, &autochpos_handler, NULL,
	  { .ref.bool_val = &cfg.auto_ch_pos },
	},
	{ "cdpath", "cd",
	  OPT_STRLIST, 0, NULL, &cdpath_handler, NULL,
	  { .ref.str_val = &cfg.cd_path },
	},
	{ "chaselinks", "",
	  OPT_BOOL, 0, NULL, &chaselinks_handler, NULL,
	  { .ref.bool_val = &cfg.chase_links },
	},
	{ "classify", "",
	  OPT_STRLIST, 0, NULL, &classify_handler, NULL,
	  { .init = &init_classify },
	},
	{ "columns", "co",
	  OPT_INT, 0, NULL, &columns_handler, NULL,
	  { .ref.int_val = &cfg.columns },
	},
	{ "confirm", "cf",
	  OPT_BOOL, 0, NULL, &confirm_handler, NULL,
	  { .ref.bool_val = &cfg.confirm },
	},
	{ "cpoptions", "cpo",
	  OPT_CHARSET, cpoptions_count, &cpoptions_vals, &cpoptions_handler, NULL,
	  { .init = &init_cpoptions },
	},
	{ "dotdirs", "",
	  OPT_SET, ARRAY_LEN(dotdirs_vals), dotdirs_vals, &dotdirs_handler, NULL,
	  { .ref.set_items = &cfg.dot_dirs },
	},
	{ "fastrun", "",
	  OPT_BOOL, 0, NULL, &fastrun_handler, NULL,
	  { .ref.bool_val = &cfg.fast_run },
	},
	{ "fillchars", "fcs",
		OPT_STRLIST, ARRAY_LEN(fillchars_enum), fillchars_enum, &fillchars_handler,
		NULL,
	  { .ref.str_val = &empty },
	},
	{ "findprg", "",
	  OPT_STR, 0, NULL, &findprg_handler, NULL,
	  { .ref.str_val = &cfg.find_prg },
	},
	{ "followlinks", "",
	  OPT_BOOL, 0, NULL, &followlinks_handler, NULL,
	  { .ref.bool_val = &cfg.follow_links },
	},
	{ "fusehome", "",
	  OPT_STR, 0, NULL, &fusehome_handler, NULL,
	  { .ref.str_val = &cfg.fuse_home },
	},
	{ "gdefault", "gd",
	  OPT_BOOL, 0, NULL, &gdefault_handler, NULL,
	  { .ref.bool_val = &cfg.gdefault },
	},
	{ "grepprg", "",
	  OPT_STR, 0, NULL, &grepprg_handler, NULL,
	  { .ref.str_val = &cfg.grep_prg },
	},
	{ "history", "hi",
	  OPT_INT, 0, NULL, &history_handler, NULL,
	  { .ref.int_val = &cfg.history_len },
	},
	{ "hlsearch", "hls",
	  OPT_BOOL, 0, NULL, &hlsearch_handler, NULL,
	  { .ref.bool_val = &cfg.hl_search },
	},
	{ "iec", "",
	  OPT_BOOL, 0, NULL, &iec_handler, NULL,
	  { .ref.bool_val = &cfg.use_iec_prefixes },
	},
	{ "ignorecase", "ic",
	  OPT_BOOL, 0, NULL, &ignorecase_handler, NULL,
	  { .ref.bool_val = &cfg.ignore_case },
	},
	{ "incsearch", "is",
	  OPT_BOOL, 0, NULL, &incsearch_handler , NULL,
	  { .ref.bool_val = &cfg.inc_search },
	},
	{ "laststatus", "ls",
	  OPT_BOOL, 0, NULL, &laststatus_handler, NULL,
	  { .ref.bool_val = &cfg.display_statusline },
	},
	{ "lines", "",
	  OPT_INT, 0, NULL, &lines_handler, NULL,
	  { .ref.int_val = &cfg.lines },
	},
	{ "locateprg", "",
	  OPT_STR, 0, NULL, &locateprg_handler, NULL,
	  { .ref.str_val = &cfg.locate_prg },
	},
	{ "mintimeoutlen", "",
	  OPT_INT, 0, NULL, &mintimeoutlen_handler, NULL,
	  { .ref.int_val = &cfg.min_timeout_len },
	},
	{ "rulerformat", "ruf",
	  OPT_STR, 0, NULL, &rulerformat_handler, NULL,
	  { .ref.str_val = &cfg.ruler_format },
	},
	{ "runexec", "",
	  OPT_BOOL, 0, NULL, &runexec_handler, NULL,
	  { .ref.bool_val = &cfg.auto_execute },
	},
	{ "scrollbind", "scb",
	  OPT_BOOL, 0, NULL, &scrollbind_handler, NULL,
	  { .ref.bool_val = &cfg.scroll_bind },
	},
	{ "scrolloff", "so",
	  OPT_INT, 0, NULL, &scrolloff_handler, NULL,
	  { .ref.int_val = &cfg.scroll_off },
	},
	{ "shell", "sh",
	  OPT_STR, 0, NULL, &shell_handler, NULL,
	  { .ref.str_val = &cfg.shell },
	},
	{ "shortmess", "shm",
	  OPT_CHARSET, shortmess_count, &shortmess_vals, &shortmess_handler, NULL,
	  { .init = &init_shortmess },
	},
#ifndef _WIN32
	{ "slowfs", "",
	  OPT_STRLIST, 0, NULL, &slowfs_handler, NULL,
	  { .ref.str_val = &cfg.slow_fs_list },
	},
#endif
	{ "smartcase", "scs",
	  OPT_BOOL, 0, NULL, &smartcase_handler, NULL,
	  { .ref.bool_val = &cfg.smart_case },
	},
	{ "sortnumbers", "",
	  OPT_BOOL, 0, NULL, &sortnumbers_handler, NULL,
	  { .ref.bool_val = &cfg.sort_numbers },
	},
	{ "statusline", "stl",
	  OPT_STR, 0, NULL, &statusline_handler, NULL,
	  { .ref.str_val = &cfg.status_line },
	},
	{ "syscalls", "",
	  OPT_BOOL, 0, NULL, &syscalls_handler, NULL,
	  { .ref.bool_val = &cfg.use_system_calls },
	},
	{ "tabstop", "ts",
	  OPT_INT, 0, NULL, &tabstop_handler, NULL,
	  { .ref.int_val = &cfg.tab_stop },
	},
	{ "timefmt", "",
	  OPT_STR, 0, NULL, &timefmt_handler, NULL,
	  { .init = &init_timefmt },
	},
	{ "timeoutlen", "tm",
	  OPT_INT, 0, NULL, &timeoutlen_handler, NULL,
	  { .ref.int_val = &cfg.timeout_len },
	},
	{ "trash", "",
	  OPT_BOOL, 0, NULL, &trash_handler, NULL,
	  { .ref.bool_val = &cfg.use_trash },
	},
	{ "trashdir", "",
	  OPT_STRLIST, 0, NULL, &trashdir_handler, NULL,
	  { .init = &init_trash_dir },
	},
	{ "tuioptions", "to",
	  OPT_CHARSET, tuioptions_count, &tuioptions_vals, &tuioptions_handler, NULL,
	  { .init = &init_tuioptions },
	},
	{ "undolevels", "ul",
	  OPT_INT, 0, NULL, &undolevels_handler, NULL,
	  { .ref.int_val = &cfg.undo_levels },
	},
	{ "vicmd", "",
	  OPT_STR, 0, NULL, &vicmd_handler, NULL,
	  { .ref.str_val = &cfg.vi_command },
	},
	{ "vixcmd", "",
	  OPT_STR, 0, NULL, &vixcmd_handler, NULL,
	  { .ref.str_val = &cfg.vi_x_command },
	},
	{ "vifminfo", "",
	  OPT_SET, ARRAY_LEN(vifminfo_set), vifminfo_set, &vifminfo_handler, NULL,
	  { .ref.set_items = &cfg.vifm_info },
	},
	{ "vimhelp", "",
	  OPT_BOOL, 0, NULL, &vimhelp_handler, NULL,
	  { .ref.bool_val = &cfg.use_vim_help },
	},
	{ "wildmenu", "wmnu",
	  OPT_BOOL, 0, NULL, &wildmenu_handler, NULL,
	  { .ref.bool_val = &cfg.wild_menu },
	},
	{ "wordchars", "",
	  OPT_STRLIST, 0, NULL, &wordchars_handler, NULL,
	  { .init = &init_wordchars },
	},
	{ "wrap", "",
	  OPT_BOOL, 0, NULL, &wrap_handler, NULL,
	  { .ref.bool_val = &cfg.wrap_quick_view },
	},
	{ "wrapscan", "ws",
	  OPT_BOOL, 0, NULL, &wrapscan_handler, NULL,
	  { .ref.bool_val = &cfg.wrap_scan },
	},

	/* Local options. */
	{ "lsview", "",
	  OPT_BOOL, 0, NULL, &lsview_global, &lsview_local,
	  { .init = &init_lsview },
	},
	{ "number", "nu",
	  OPT_BOOL, 0, NULL, &number_global, &number_local,
	  { .init = &init_number },
	},
	{ "numberwidth", "nuw",
	  OPT_INT, 0, NULL, &numberwidth_global, &numberwidth_local,
	  { .init = &init_numberwidth },
	},
	{ "relativenumber", "rnu",
	  OPT_BOOL, 0, NULL, &relativenumber_global, &relativenumber_local,
	  { .init = &init_relativenumber },
	},
	{ "sort", "",
	  OPT_STRLIST, ARRAY_LEN(sort_types), sort_types, &sort_global, &sort_local,
	  { .init = &init_sort },
	},
	{ "sortorder", "",
	  OPT_ENUM, ARRAY_LEN(sortorder_enum), sortorder_enum, &sortorder_global,
		&sortorder_local,
	  { .init = &init_sortorder },
	},
	{ "viewcolumns", "",
	  OPT_STRLIST, 0, NULL, &viewcolumns_global, &viewcolumns_local,
	  { .init = &init_viewcolumns },
	},
};

static int error;

void
init_option_handlers(void)
{
	static int opt_changed;
	init_options(&opt_changed);
	load_options_defaults();
	add_options();
}

/* Composes the default value for the 'classify' option. */
static void
init_classify(optval_t *val)
{
	val->str_val = (char *)classify_to_str();
}

const char *
classify_to_str(void)
{
	static char buf[64];
	size_t len = 0;
	int filetype;
	buf[0] = '\0';
	for(filetype = 0; filetype < FT_COUNT; ++filetype)
	{
		const char prefix[2] = { cfg.decorations[filetype][DECORATION_PREFIX] };
		const char suffix[2] = { cfg.decorations[filetype][DECORATION_SUFFIX] };
		if(prefix[0] != '\0' || suffix[0] != '\0')
		{
			if(buf[0] != '\0')
			{
				(void)strncat(buf + len, ",", sizeof(buf) - len - 1);
				len += strlen(buf + len);
			}
			(void)snprintf(buf + len, sizeof(buf) - len, "%s:%s:%s", prefix,
					get_type_str(filetype), suffix);
			len += strlen(buf + len);
		}
	}
	return buf;
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
init_trash_dir(optval_t *val)
{
	val->str_val = cfg.trash_dir;
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
			cfg.filelist_col_padding ? "p" : "",
			cfg.side_borders_visible ? "s" : "");
	val->str_val = buf;
}

static void
init_viewcolumns(optval_t *val)
{
	val->str_val = "";
}

/* Formats 'wordchars' initial value from cfg.word_chars array. */
static void
init_wordchars(optval_t *val)
{
	static char *str;

	size_t i;
	size_t len;

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
		else
		{
			options[i].val.str_val = *options[i].initializer.ref.str_val;
		}
	}
}

static void
add_options(void)
{
	size_t i;
	for(i = 0U; i < ARRAY_LEN(options); ++i)
	{
		add_option(options[i].name, options[i].abbr, options[i].type, OPT_GLOBAL,
				options[i].val_count, options[i].vals, options[i].global_handler,
				options[i].val);

		if(options[i].local_handler != NULL)
		{
			add_option(options[i].name, options[i].abbr, options[i].type, OPT_LOCAL,
					options[i].val_count, options[i].vals, options[i].local_handler,
					options[i].val);
		}
	}
}

void
reset_local_options(FileView *view)
{
	optval_t val;

	memcpy(view->sort, view->sort_g, sizeof(view->sort));
	load_sort_option_inner(view, view->sort);

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
clone_local_options(const FileView *from, FileView *to)
{
	replace_string(&to->view_columns, from->view_columns);
	replace_string(&to->view_columns_g, from->view_columns_g);
	to->num_width = from->num_width;
	to->num_width_g = from->num_width_g;
	to->num_type = from->num_type;
	to->num_type_g = from->num_type_g;

	memcpy(to->sort, from->sort, sizeof(to->sort));
	memcpy(to->sort_g, from->sort_g, sizeof(to->sort_g));
	to->ls_view_g = from->ls_view_g;
	fview_set_lsview(to, from->ls_view);
}

void
load_sort_option(FileView *view)
{
	load_sort_option_inner(view, view->sort);
	load_sort_option_inner(view, view->sort_g);
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

	opt_val[0] = '\0';

	ui_view_sort_list_ensure_well_formed(view, sort_keys);

	/* Produce a string, which represents a list of sorting keys. */
	i = -1;
	while(++i < SK_COUNT && abs(sort_keys[i]) <= SK_LAST)
	{
		const int sort_option = sort_keys[i];
		const char *const comma = (opt_val_len == 0U) ? "" : ",";
		const char option_mark = (sort_option < 0) ? '-' : '+';
		const char *const option_name = sort_enum[abs(sort_option) - 1];

		opt_val_len += snprintf(opt_val + opt_val_len,
				sizeof(opt_val) - opt_val_len, "%s%c%s", comma, option_mark,
				option_name);
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
	set_options_error = 0;
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
		clean_positions_in_history(curr_view);
		clean_positions_in_history(other_view);
	}
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
	char decorations[FT_COUNT][2] = {};

	if(str_to_classify(val.str_val, decorations) == 0)
	{
		assert(sizeof(cfg.decorations) == sizeof(decorations) && "Arrays diverged");
		memcpy(&cfg.decorations, &decorations, sizeof(cfg.decorations));

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
str_to_classify(const char str[], char decorations[FT_COUNT][2])
{
	char *saveptr;
	char *str_copy;
	char *token;
	int error_encountered;

	str_copy = strdup(str);
	if(str_copy == NULL)
	{
		/* Not enough memory. */
		return 1;
	}

	error_encountered = 0;
	for(token = str_copy; (token = strtok_r(token, ",", &saveptr)); token = NULL)
	{
		FileType type;
		const char *suffix = pick_out_decoration(token, &type);
		if(suffix == NULL)
		{
			vle_tb_append_linef(vle_err, "Invalid filetype: %s", token);
			error_encountered = 1;
		}
		else
		{
			if(strlen(token) > 1)
			{
				vle_tb_append_linef(vle_err, "Invalid prefix: %s", token);
				error_encountered = 1;
			}
			if(strlen(suffix) > 1)
			{
				vle_tb_append_linef(vle_err, "Invalid suffix: %s", suffix);
				error_encountered = 1;
			}
		}

		if(!error_encountered)
		{
			decorations[type][DECORATION_PREFIX] = token[0];
			decorations[type][DECORATION_SUFFIX] = suffix[0];
		}
	}
	free(str_copy);

	return error_encountered;
}

/* Puts '\0' after prefix end and returns pointer to the suffix beginning or
 * NULL on unknown filetype specifier. */
static const char *
pick_out_decoration(const char classify_item[], FileType *type)
{
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
	return NULL;
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
	cfg.confirm = val.bool_val;
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
	cfg.use_iec_prefixes = val.bool_val;

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
		*endpoint = **str;
		++*str;
	}
	return 0;
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

/* Handles switch that controls column vs. ls-like view in global option. */
static void
lsview_global(OPT_OP op, optval_t val)
{
	curr_view->ls_view_g = val.bool_val;
	if(curr_stats.global_local_settings)
	{
		other_view->ls_view_g = val.bool_val;
	}
}

/* Handles switch that controls column vs. ls-like view in local option. */
static void
lsview_local(OPT_OP op, optval_t val)
{
	fview_set_lsview(curr_view, val.bool_val);
	if(curr_stats.global_local_settings)
	{
		fview_set_lsview(other_view, val.bool_val);
	}
}

/* Handles file numbers displaying toggle in global option. */
static void
number_global(OPT_OP op, optval_t val)
{
	update_num_type(curr_view, &curr_view->num_type_g, NT_SEQ, val.bool_val);
	if(curr_stats.global_local_settings)
	{
		update_num_type(other_view, &other_view->num_type_g, NT_SEQ, val.bool_val);
	}
}

/* Handles file numbers displaying toggle in local option. */
static void
number_local(OPT_OP op, optval_t val)
{
	update_num_type(curr_view, &curr_view->num_type, NT_SEQ, val.bool_val);
	if(curr_stats.global_local_settings)
	{
		update_num_type(other_view, &other_view->num_type, NT_SEQ, val.bool_val);
	}
}

/* Handles changes of minimum width of file number field in global option. */
static void
numberwidth_global(OPT_OP op, optval_t val)
{
	set_numberwidth(curr_view, &curr_view->num_width_g, val.int_val);
	if(curr_stats.global_local_settings)
	{
		set_numberwidth(other_view, &other_view->num_width_g, val.int_val);
	}
}

/* Handles changes of minimum width of file number field in local option. */
static void
numberwidth_local(OPT_OP op, optval_t val)
{
	set_numberwidth(curr_view, &curr_view->num_width, val.int_val);
	if(curr_stats.global_local_settings)
	{
		set_numberwidth(other_view, &other_view->num_width, val.int_val);
	}
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
	if(curr_stats.global_local_settings)
	{
		update_num_type(other_view, &other_view->num_type_g, NT_REL, val.bool_val);
	}
}

/* Handles relative file numbers displaying toggle in local option. */
static void
relativenumber_local(OPT_OP op, optval_t val)
{
	update_num_type(curr_view, &curr_view->num_type, NT_REL, val.bool_val);
	if(curr_stats.global_local_settings)
	{
		update_num_type(other_view, &other_view->num_type, NT_REL, val.bool_val);
	}
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
	if(curr_stats.global_local_settings)
	{
		set_sort(other_view, other_view->sort_g, val.str_val);
	}
}

/* Handler for local 'sort' option, parses the value and checks it for
 * correctness. */
static void
sort_local(OPT_OP op, optval_t val)
{
	set_sort(curr_view, curr_view->sort, val.str_val);
	if(curr_stats.global_local_settings)
	{
		set_sort(other_view, other_view->sort, val.str_val);
	}
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

		pos = string_array_pos((char **)sort_enum, ARRAY_LEN(sort_enum), name);
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
		/* Reset search results, which might be outdated after resorting. */
		view->matches = 0;
		fview_sorting_updated(view);
		resort_view(view);
		fview_cursor_redraw(view);
	}

	load_sort_option_inner(view, sort_keys);
}

/* Handles global 'sortorder' option and corrects ordering for primary sorting
 * key. */
static void
sortorder_global(OPT_OP op, optval_t val)
{
	set_sortorder(curr_view, (val.enum_item == 1) ? 0 : 1, curr_view->sort_g);
	if(curr_stats.global_local_settings)
	{
		set_sortorder(other_view, (val.enum_item == 1) ? 0 : 1, other_view->sort_g);
	}
}

/* Handles local 'sortorder' option and corrects ordering for primary sorting
 * key. */
static void
sortorder_local(OPT_OP op, optval_t val)
{
	set_sortorder(curr_view, (val.enum_item == 1) ? 0 : 1, curr_view->sort);
	if(curr_stats.global_local_settings)
	{
		set_sortorder(other_view, (val.enum_item == 1) ? 0 : 1, other_view->sort);
	}
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
	if(curr_stats.global_local_settings)
	{
		replace_string(&other_view->view_columns_g, val.str_val);
	}
}

/* Handler of local 'viewcolumns' option, which defines custom view columns. */
static void
viewcolumns_local(OPT_OP op, optval_t val)
{
	set_viewcolumns(curr_view, val.str_val);
	if(curr_stats.global_local_settings)
	{
		set_viewcolumns(other_view, val.str_val);
	}
}

/* Setups view columns for the view. */
static void
set_viewcolumns(FileView *view, const char view_columns[])
{
	const int update_columns_ui = ui_view_displays_columns(view);
	set_view_columns_option(view, view_columns, update_columns_ui);
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
	const columns_t columns = update_ui ? view->columns : NULL;

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
			(void)parse_columns(columns, &add_column, &map_name, view->view_columns,
					view);
		}

		val.str_val = view->view_columns;
		set_option("viewcolumns", val, OPT_GLOBAL);
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
add_column(columns_t columns, column_info_t column_info)
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
	FileView *view = arg;
	if(*name != '\0')
	{
		int pos;
		pos = string_array_pos((char **)sort_enum, ARRAY_LEN(sort_enum), name);
		return (pos >= 0) ? (pos + 1) : -1;
	}
	return (int)get_secondary_key((SortingKey)abs(view->sort[0]));
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
	draw_dir_list(view);
	refresh_view_win(view);
}

static void
statusline_handler(OPT_OP op, optval_t val)
{
	(void)replace_string(&cfg.status_line, val.str_val);
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
	cfg.filelist_col_padding = 0;
	cfg.side_borders_visible = 0;

	/* And set the ones present in the value. */
	p = val.str_val;
	while(*p != '\0')
	{
		switch(*p)
		{
			case 'p':
				cfg.filelist_col_padding = 1;
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

		while(from < to)
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
		quick_view_file(curr_view);
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
