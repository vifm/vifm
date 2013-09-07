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

#include <assert.h> /* assert() */
#include <limits.h> /* INT_MIN */
#include <math.h> /* abs() */
#include <stddef.h> /* NULL */
#include <stdio.h> /* snprintf() */
#include <stdlib.h>
#include <string.h> /* memcpy() memset() strstr() */

#include "cfg/config.h"
#include "engine/options.h"
#include "engine/text_buffer.h"
#include "modes/view.h"
#include "utils/log.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/utils.h"
#include "color_scheme.h"
#include "column_view.h"
#include "filelist.h"
#include "quickview.h"
#include "sort.h"
#include "status.h"
#include "ui.h"
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
}optvalref_t;

typedef void (*optinit_func)(optval_t *val);

typedef struct
{
	optvalref_t ref;
	optinit_func init;
}optinit_t;

static void init_classify(optval_t *val);
static void init_cpoptions(optval_t *val);
static void init_timefmt(optval_t *val);
static void init_trash_dir(optval_t *val);
static void init_lsview(optval_t *val);
static void init_shortmess(optval_t *val);
static void init_sort(optval_t *val);
static void init_sortorder(optval_t *val);
static void init_viewcolumns(optval_t *val);
static void load_options_defaults(void);
static void add_options(void);
static void apropos_handler(OPT_OP op, optval_t val);
static void autochpos_handler(OPT_OP op, optval_t val);
static void classify_handler(OPT_OP op, optval_t val);
static int str_to_classify(const char str[],
		char decorations[FILE_TYPE_COUNT][2]);
static const char * pick_out_decoration(const char classify_item[],
		FileType *type);
static void columns_handler(OPT_OP op, optval_t val);
static void confirm_handler(OPT_OP op, optval_t val);
static void cpoptions_handler(OPT_OP op, optval_t val);
static void dotdirs_handler(OPT_OP op, optval_t val);
static void fastrun_handler(OPT_OP op, optval_t val);
static void findprg_handler(OPT_OP op, optval_t val);
static void followlinks_handler(OPT_OP op, optval_t val);
static void fusehome_handler(OPT_OP op, optval_t val);
static void gdefault_handler(OPT_OP op, optval_t val);
static void grep_handler(OPT_OP op, optval_t val);
static void history_handler(OPT_OP op, optval_t val);
static void hlsearch_handler(OPT_OP op, optval_t val);
static void iec_handler(OPT_OP op, optval_t val);
static void ignorecase_handler(OPT_OP op, optval_t val);
static void incsearch_handler(OPT_OP op, optval_t val);
static void laststatus_handler(OPT_OP op, optval_t val);
static void lines_handler(OPT_OP op, optval_t val);
static void locate_handler(OPT_OP op, optval_t val);
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
static void lsview_handler(OPT_OP op, optval_t val);
static void sort_handler(OPT_OP op, optval_t val);
static void sortorder_handler(OPT_OP op, optval_t val);
static void viewcolumns_handler(OPT_OP op, optval_t val);
static void add_column(columns_t columns, column_info_t column_info);
static int map_name(const char *name);
static void resort_view(FileView * view);
static void statusline_handler(OPT_OP op, optval_t val);
static void tabstop_handler(OPT_OP op, optval_t val);
static void timefmt_handler(OPT_OP op, optval_t val);
static void timeoutlen_handler(OPT_OP op, optval_t val);
static void trash_handler(OPT_OP op, optval_t val);
static void trashdir_handler(OPT_OP op, optval_t val);
static void undolevels_handler(OPT_OP op, optval_t val);
static void vicmd_handler(OPT_OP op, optval_t val);
static void vixcmd_handler(OPT_OP op, optval_t val);
static void vifminfo_handler(OPT_OP op, optval_t val);
static void vimhelp_handler(OPT_OP op, optval_t val);
static void wildmenu_handler(OPT_OP op, optval_t val);
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
};
ARRAY_GUARD(sort_enum, SORT_OPTION_COUNT);

static const char cpoptions_list[] = "fst";
static const char * cpoptions_vals = cpoptions_list;
#define cpoptions_count ARRAY_LEN(cpoptions_list)

static const char * dotdirs_vals[] = {
	"rootparent",
	"nonrootparent",
};
ARRAY_GUARD(dotdirs_vals, NUM_DOT_DIRS);

static const char shortmess_list[] = "T";
static const char * shortmess_vals = shortmess_list;
#define shortmess_count ARRAY_LEN(shortmess_list)

static const char * sort_types[] = {
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
};
ARRAY_GUARD(sort_types, SORT_OPTION_COUNT*3);

static const char * sortorder_enum[] = {
	"ascending",
	"descending",
};

static const char * vifminfo_set[] = {
	"options",
	"filetypes",
	"commands",
	"bookmarks",
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

static struct
{
	const char *name;
	const char *abbr;
	OPT_TYPE type;
	int val_count;
	const char **vals;
	opt_handler handler;
	optinit_t initializer;
	optval_t val;
}options[] = {
	/* global options */
	{ "aproposprg",   "",    OPT_STR,     0,                          NULL,            &apropos_handler,
		{ .ref.str_val = &cfg.apropos_prg }                                                                   },
	{ "autochpos",   "",     OPT_BOOL,    0,                          NULL,            &autochpos_handler,
		{ .ref.bool_val = &cfg.auto_ch_pos }                                                                   },
	{ "classify",    "",     OPT_STRLIST, 0,                          NULL,            &classify_handler,
		{ .init = &init_classify }                                                                             },
	{ "columns",     "co",   OPT_INT,     0,                          NULL,            &columns_handler,
		{ .ref.int_val = &cfg.columns }                                                                        },
	{ "confirm",     "cf",   OPT_BOOL,    0,                          NULL,            &confirm_handler,
		{ .ref.bool_val = &cfg.confirm }                                                                       },
	{ "cpoptions",   "cpo",  OPT_CHARSET, cpoptions_count,            &cpoptions_vals, &cpoptions_handler,
		{ .init = &init_cpoptions }                                                                            },
	{ "dotdirs",     "",     OPT_SET,     ARRAY_LEN(dotdirs_vals),    dotdirs_vals,    &dotdirs_handler,
		{ .ref.set_items = &cfg.dot_dirs }                                                                     },
	{ "fastrun",     "",     OPT_BOOL,    0,                          NULL,            &fastrun_handler,
		{ .ref.bool_val = &cfg.fast_run }                                                                      },
	{ "findprg",     "",     OPT_STR,     0,                          NULL,            &findprg_handler,
		{ .ref.str_val = &cfg.find_prg }                                                                       },
	{ "followlinks", "",     OPT_BOOL,    0,                          NULL,            &followlinks_handler,
		{ .ref.bool_val = &cfg.follow_links }                                                                  },
	{ "fusehome",    "",     OPT_STR,     0,                          NULL,            &fusehome_handler,
		{ .ref.str_val = &cfg.fuse_home }                                                                      },
	{ "gdefault",    "gd",   OPT_BOOL,    0,                          NULL,            &gdefault_handler,
		{ .ref.bool_val = &cfg.gdefault }                                                                      },
	{ "grepprg",     "",     OPT_STR,     0,                          NULL,            &grep_handler,
		{ .ref.str_val = &cfg.grep_prg }                                                                       },
	{ "history",     "hi",   OPT_INT,     0,                          NULL,            &history_handler,
		{ .ref.int_val = &cfg.history_len }                                                                    },
	{ "hlsearch",    "hls",  OPT_BOOL,    0,                          NULL,            &hlsearch_handler,
		{ .ref.bool_val = &cfg.hl_search }                                                                     },
	{ "iec",         "",     OPT_BOOL,    0,                          NULL,            &iec_handler,
		{ .ref.bool_val = &cfg.use_iec_prefixes }                                                              },
	{ "ignorecase",  "ic",   OPT_BOOL,    0,                          NULL,            &ignorecase_handler,
		{ .ref.bool_val = &cfg.ignore_case }                                                                   },
	{ "incsearch",   "is",   OPT_BOOL,    0,                          NULL,            &incsearch_handler ,
		{ .ref.bool_val = &cfg.inc_search }                                                                    },
	{ "laststatus",  "ls",   OPT_BOOL,    0,                          NULL,            &laststatus_handler,
		{ .ref.bool_val = &cfg.last_status }                                                                   },
	{ "lines",       "",     OPT_INT,     0,                          NULL,            &lines_handler,
		{ .ref.int_val = &cfg.lines }                                                                          },
	{ "locateprg",   "",     OPT_STR,     0,                          NULL,            &locate_handler,
		{ .ref.str_val = &cfg.locate_prg }                                                                     },
	{ "rulerformat", "ruf",  OPT_STR,     0,                          NULL,            &rulerformat_handler,
		{ .ref.str_val = &cfg.ruler_format }                                                                   },
	{ "runexec",     "",     OPT_BOOL,    0,                          NULL,            &runexec_handler,
		{ .ref.bool_val = &cfg.auto_execute }                                                                  },
	{ "scrollbind",  "scb",  OPT_BOOL,    0,                          NULL,            &scrollbind_handler,
		{ .ref.bool_val = &cfg.scroll_bind }                                                                   },
	{ "scrolloff",   "so",   OPT_INT,     0,                          NULL,            &scrolloff_handler,
		{ .ref.int_val = &cfg.scroll_off }                                                                     },
	{ "shell",       "sh",   OPT_STR,     0,                          NULL,            &shell_handler,
		{ .ref.str_val = &cfg.shell }                                                                          },
	{ "shortmess",   "shm",  OPT_CHARSET, shortmess_count,            &shortmess_vals, &shortmess_handler,
		{ .init = &init_shortmess }                                                                            },
#ifndef _WIN32
	{ "slowfs",      "",     OPT_STRLIST, 0,                          NULL,            &slowfs_handler,
		{ .ref.str_val = &cfg.slow_fs_list }                                                                   },
#endif
	{ "smartcase",   "scs",  OPT_BOOL,    0,                          NULL,            &smartcase_handler,
		{ .ref.bool_val = &cfg.smart_case }                                                                    },
	{ "sortnumbers", "",     OPT_BOOL,    0,                          NULL,            &sortnumbers_handler,
		{ .ref.bool_val = &cfg.sort_numbers }                                                                  },
	{ "statusline",  "stl",  OPT_STR,     0,                          NULL,            &statusline_handler,
		{ .ref.str_val = &cfg.status_line }                                                                    },
	{ "tabstop",     "ts",   OPT_INT,     0,                          NULL,            &tabstop_handler,
		{ .ref.int_val = &cfg.tab_stop }                                                                       },
	{ "timefmt",     "",     OPT_STR,     0,                          NULL,            &timefmt_handler,
		{ .init = &init_timefmt }                                                                              },
	{ "timeoutlen",  "tm",   OPT_INT,     0,                          NULL,            &timeoutlen_handler,
		{ .ref.int_val = &cfg.timeout_len }                                                                    },
	{ "trash",       "",     OPT_BOOL,    0,                          NULL,            &trash_handler,
		{ .ref.bool_val = &cfg.use_trash }                                                                     },
	{ "trashdir",    "",     OPT_STR,     0,                          NULL,            &trashdir_handler,
		{ .init = &init_trash_dir }                                                                            },
	{ "undolevels",  "ul",   OPT_INT,     0,                          NULL,            &undolevels_handler,
		{ .ref.int_val = &cfg.undo_levels }                                                                    },
	{ "vicmd",       "",     OPT_STR,     0,                          NULL,            &vicmd_handler,
		{ .ref.str_val = &cfg.vi_command }                                                                     },
	{ "vixcmd",      "",     OPT_STR,     0,                          NULL,            &vixcmd_handler,
		{ .ref.str_val = &cfg.vi_x_command }                                                                   },
	{ "vifminfo",    "",     OPT_SET,     ARRAY_LEN(vifminfo_set),    vifminfo_set,    &vifminfo_handler,
		{ .ref.set_items = &cfg.vifm_info }                                                                    },
	{ "vimhelp",     "",     OPT_BOOL,    0,                          NULL,            &vimhelp_handler,
		{ .ref.bool_val = &cfg.use_vim_help }                                                                  },
	{ "wildmenu",    "wmnu", OPT_BOOL,    0,                          NULL,            &wildmenu_handler,
		{ .ref.bool_val = &cfg.wild_menu }                                                                     },
	{ "wrap",        "",     OPT_BOOL,    0,                          NULL,            &wrap_handler,
		{ .ref.bool_val = &cfg.wrap_quick_view }                                                               },
	{ "wrapscan",    "ws",   OPT_BOOL,    0,                          NULL,            &wrapscan_handler,
		{ .ref.bool_val = &cfg.wrap_scan }                                                                     },
	/* local options */
	{ "lsview",      "",     OPT_BOOL,    0,                          NULL,            &lsview_handler,
		{ .init = &init_lsview }                                                                               },
	{ "sort",        "",     OPT_STRLIST, ARRAY_LEN(sort_types),      sort_types,      &sort_handler,
		{ .init = &init_sort }                                                                                 },
	{ "sortorder",   "",     OPT_ENUM,    ARRAY_LEN(sortorder_enum),  sortorder_enum,  &sortorder_handler,
		{ .init = &init_sortorder }                                                                            },
	{ "viewcolumns", "",     OPT_STRLIST, 0,                          NULL,            &viewcolumns_handler,
		{ .init = &init_viewcolumns }                                                                          },
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
	for(filetype = 0; filetype < FILE_TYPE_COUNT; filetype++)
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

static void
init_cpoptions(optval_t *val)
{
	static char buf[32];
	snprintf(buf, sizeof(buf), "%s%s%s",
			cfg.filter_inverted_by_default ? "f" : "",
			cfg.selection_is_primary ? "s" : "", cfg.tab_switches_pane ? "t" : "");
	val->str_val = buf;
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
	val->bool_val = curr_view->ls_view;
}

static void
init_shortmess(optval_t *val)
{
	static char buf[32];
	snprintf(buf, sizeof(buf), "%s", cfg.trunc_normal_sb_msgs ? "T" : "");
	val->str_val = buf;
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

static void
init_viewcolumns(optval_t *val)
{
	val->str_val = "";
}

static void
load_options_defaults(void)
{
	size_t i;
	for(i = 0; i < ARRAY_LEN(options); i++)
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
	int i;
	for(i = 0; i < ARRAY_LEN(options); i++)
	{
		add_option(options[i].name, options[i].abbr, options[i].type,
				options[i].val_count, options[i].vals, options[i].handler,
				options[i].val);
	}
}

void
load_local_options(FileView *view)
{
	optval_t val;

	load_sort_option(view);

	val.str_val = view->view_columns;
	set_option("viewcolumns", val);

	val.bool_val = view->ls_view;
	set_option("lsview", val);
}

void
load_sort_option(FileView *view)
{
	/* This approximate maximum length also includes "+" or "-" sign and a
	 * comma (",") between items. */
	enum { MAX_SORT_OPTION_NAME_LEN = 16 };

	optval_t val;
	char opt_val[MAX_SORT_OPTION_NAME_LEN*SORT_OPTION_COUNT] = "";
	size_t opt_val_len = 0U;
	int j, i;

	/* Ensure that list of sorting keys contains either "name" or "iname". */
	j = -1;
	while(++j < SORT_OPTION_COUNT && abs(view->sort[j]) <= LAST_SORT_OPTION)
	{
		const int sort_option = abs(view->sort[j]);
		if(sort_option == SORT_BY_NAME || sort_option == SORT_BY_INAME)
		{
			break;
		}
	}
	if(j < SORT_OPTION_COUNT && abs(view->sort[j]) > LAST_SORT_OPTION)
	{
		view->sort[j++] = DEFAULT_SORT_KEY;
	}

	/* Produce a string, which represents a list of sorting keys. */
	i = -1;
	while(++i < SORT_OPTION_COUNT && abs(view->sort[i]) <= LAST_SORT_OPTION)
	{
		const int sort_option = view->sort[i];
		const char *const comma = (opt_val_len == 0U) ? "" : ",";
		const char option_mark = (sort_option < 0) ? '-' : '+';
		const char *const option_name = sort_enum[abs(sort_option) - 1];

		opt_val_len += snprintf(opt_val + opt_val_len,
				sizeof(opt_val) - opt_val_len, "%s%c%s", comma, option_mark,
				option_name);
	}

	val.str_val = opt_val;
	set_option("sort", val);

	val.enum_item = (view->sort[0] < 0);
	set_option("sortorder", val);
}

int
process_set_args(const char *args)
{
	int set_options_error;
	const char *text_buffer;

	text_buffer_clear();

	/* Call of set_options() can change error. */
	error = 0;
	set_options_error = set_options(args) != 0;
	error = error || set_options_error;
	text_buffer = text_buffer_get();

	if(error)
	{
		text_buffer_add("Invalid argument for :set command");
		/* We just modified text buffer and can't use text_buffer variable. */
		status_bar_error(text_buffer_get());
	}
	else if(text_buffer[0] != '\0')
	{
		status_bar_message(text_buffer);
	}
	return error ? -1 : (text_buffer[0] != '\0');
}

static void
apropos_handler(OPT_OP op, optval_t val)
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

static void
classify_handler(OPT_OP op, optval_t val)
{
	char decorations[FILE_TYPE_COUNT][2] = {};

	if(str_to_classify(val.str_val, decorations) == 0)
	{
		assert(sizeof(cfg.decorations) == sizeof(decorations) && "Arrays diverged.");
		memcpy(&cfg.decorations, &decorations, sizeof(cfg.decorations));

		update_screen(UT_REDRAW);
	}
	else
	{
		error = 1;
	}

	init_classify(&val);
	set_option("classify", val);
}

/* Fills the decorations array with parsed classification values from the str.
 * It's assumed that decorations array is zeroed.  Returns zero on success,
 * otherwise non-zero is returned. */
static int
str_to_classify(const char str[], char decorations[FILE_TYPE_COUNT][2])
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
			text_buffer_addf("Invalid filetype: %s", token);
			error_encountered = 1;
		}
		else
		{
			if(strlen(token) > 1)
			{
				text_buffer_addf("Invalid prefix: %s", token);
				error_encountered = 1;
			}
			if(strlen(suffix) > 1)
			{
				text_buffer_addf("Invalid suffix: %s", suffix);
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
	for(filetype = 0; filetype < FILE_TYPE_COUNT; filetype++)
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
		text_buffer_addf("At least %d columns needed", MIN_TERM_WIDTH);
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
	set_option("columns", val);
}

static void
confirm_handler(OPT_OP op, optval_t val)
{
	cfg.confirm = val.bool_val;
}

static void
cpoptions_handler(OPT_OP op, optval_t val)
{
	char *p;

	cfg.filter_inverted_by_default = 0;
	cfg.selection_is_primary = 0;
	cfg.tab_switches_pane = 0;

	p = val.str_val;
	while(*p != '\0')
	{
		if(*p == 'f')
		{
			cfg.filter_inverted_by_default = 1;
		}
		else if(*p == 's')
		{
			cfg.selection_is_primary = 1;
		}
		else if(*p == 't')
		{
			cfg.tab_switches_pane = 1;
		}
		p++;
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
	if(set_fuse_home(expanded_path) != 0)
	{
		/* Reset the 'fusehome' options to its previous value. */
		val.str_val = cfg.fuse_home;
		set_option("fusehome", val);
	}
	free(expanded_path);
}

static void
gdefault_handler(OPT_OP op, optval_t val)
{
	cfg.gdefault = val.bool_val;
}

static void
grep_handler(OPT_OP op, optval_t val)
{
	(void)replace_string(&cfg.grep_prg, val.str_val);
}

static void
history_handler(OPT_OP op, optval_t val)
{
	resize_history(MAX(0, val.int_val));
}

static void
hlsearch_handler(OPT_OP op, optval_t val)
{
	cfg.hl_search = val.bool_val;
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

static void
laststatus_handler(OPT_OP op, optval_t val)
{
	cfg.last_status = val.bool_val;
	if(cfg.last_status)
	{
		if(curr_stats.split == VSPLIT)
			scroll_line_down(&lwin);
		scroll_line_down(&rwin);
	}
	else
	{
		if(curr_stats.split == VSPLIT)
			lwin.window_rows++;
		rwin.window_rows++;
		wresize(lwin.win, lwin.window_rows + 1, lwin.window_width + 1);
		wresize(rwin.win, rwin.window_rows + 1, rwin.window_width + 1);
		draw_dir_list(&lwin);
		draw_dir_list(&rwin);
	}
	move_to_list_pos(curr_view, curr_view->list_pos);
	touchwin(lwin.win);
	touchwin(rwin.win);
	touchwin(lborder);
	if(curr_stats.split == VSPLIT)
		touchwin(mborder);
	touchwin(rborder);
	wnoutrefresh(lwin.win);
	wnoutrefresh(rwin.win);
	wnoutrefresh(lborder);
	wnoutrefresh(mborder);
	wnoutrefresh(rborder);
	doupdate();
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
		text_buffer_addf("At least %d lines needed", MIN_TERM_HEIGHT);
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
	set_option("lines", val);
}

static void
locate_handler(OPT_OP op, optval_t val)
{
	(void)replace_string(&cfg.locate_prg, val.str_val);
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
		text_buffer_addf("Invalid scroll size: %d", val.int_val);
		error = 1;
		reset_option_to_default("scrolloff");
		return;
	}

	cfg.scroll_off = val.int_val;
	if(cfg.scroll_off > 0)
		move_to_list_pos(curr_view, curr_view->list_pos);
}

static void
shell_handler(OPT_OP op, optval_t val)
{
	(void)replace_string(&cfg.shell, val.str_val);
}

static void
shortmess_handler(OPT_OP op, optval_t val)
{
	const char *p;

	cfg.trunc_normal_sb_msgs = 0;

	p = val.str_val;
	while(*p != '\0')
	{
		if(*p == 'T')
		{
			cfg.trunc_normal_sb_msgs = 1;
		}
		p++;
	}
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

static void
lsview_handler(OPT_OP op, optval_t val)
{
	curr_view->ls_view = val.bool_val;

	if(val.bool_val)
	{
		column_info_t column_info =
		{
			.column_id = SORT_BY_NAME, .full_width = 0UL, .text_width = 0UL,
			.align = AT_LEFT,          .sizing = ST_AUTO, .cropping = CT_ELLIPSIS,
		};

		columns_clear(curr_view->columns);
		columns_add_column(curr_view->columns, column_info);
		redraw_current_view();
	}
	else
	{
		load_view_columns_option(curr_view, curr_view->view_columns);
	}
}

static void
sort_handler(OPT_OP op, optval_t val)
{
	char *p = val.str_val - 1;
	int i, j;

	i = 0;
	do
	{
		char buf[32];
		char *t;
		int minus;
		int pos;

		t = p + 1;
		p = strchr(t, ',');
		if(p == NULL)
			p = t + strlen(t);

		minus = (*t == '-');
		if(*t == '-' || *t == '+')
			t++;

		snprintf(buf, MIN(p - t + 1, sizeof(buf)), "%s", t);

		if((pos = string_array_pos((char **)sort_enum, ARRAY_LEN(sort_enum),
				buf)) == -1)
			continue;
		pos++;

		for(j = 0; j < i; j++)
			if(abs(curr_view->sort[j]) == pos)
				break;

		if(minus)
			curr_view->sort[j] = -pos;
		else
			curr_view->sort[j] = pos;
		if(j == i)
			i++;
	}
	while(*p != '\0');

	for(j = 0; j < i; j++)
	{
		int sort_key = abs(curr_view->sort[j]);
		if(sort_key == SORT_BY_NAME || sort_key == SORT_BY_INAME)
			break;
	}
	if(j == i)
	{
		curr_view->sort[i++] = DEFAULT_SORT_KEY;
	}
	memset(&curr_view->sort[i], NO_SORT_OPTION, sizeof(curr_view->sort) - i);

	reset_view_sort(curr_view);
	resort_view(curr_view);
	move_to_list_pos(curr_view, curr_view->list_pos);
	load_sort_option(curr_view);
}

static void
sortorder_handler(OPT_OP op, optval_t val)
{
	if((val.enum_item ? -1 : +1)*curr_view->sort[0] < 0)
	{
		curr_view->sort[0] = -curr_view->sort[0];

		resort_view(curr_view);
		load_sort_option(curr_view);
	}
}

/* Handler of local to a view 'viewcolumns' option, which defines custom view
 * columns. */
static void
viewcolumns_handler(OPT_OP op, optval_t val)
{
	load_view_columns_option(curr_view, val.str_val);
}

void
load_view_columns_option(FileView *view, const char value[])
{
	const char *new_value = (value[0] == '\0') ? DEFAULT_VIEW_COLUMNS : value;

	columns_clear(curr_view->columns);
	if(parse_columns(view->columns, add_column, map_name, new_value) != 0)
	{
		text_buffer_add("Invalid format of 'viewcolumns' option");
		error = 1;
		(void)parse_columns(view->columns, add_column, map_name,
				view->view_columns);
	}
	else
	{
		/* Set value specified by user.  Can't use DEFAULT_VIEW_COLUMNS here,
		 * because empty value of view->view->columns signals about disabled
		 * columns customization. */
		(void)replace_string(&view->view_columns, value);
		redraw_current_view();
	}
}

/* Adds new column to view columns. */
static void
add_column(columns_t columns, column_info_t column_info)
{
	columns_add_column(columns, column_info);
}

/* Maps column name to column id. Returns column id. */
static int
map_name(const char *name)
{
	if(*name != '\0')
	{
		int pos;
		pos = string_array_pos((char **)sort_enum, ARRAY_LEN(sort_enum), name);
		return (pos >= 0) ? (pos + 1) : -1;
	}
	return get_secondary_key(abs(curr_view->sort[0]));
}

void
load_geometry(void)
{
	optval_t val;
	val.int_val = cfg.columns;
	set_option("columns", val);
	val.int_val = cfg.lines;
	set_option("lines", val);
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

static void
tabstop_handler(OPT_OP op, optval_t val)
{
	if(val.int_val <= 0)
	{
		text_buffer_addf("Argument must be positive: %d", val.int_val);
		error = 1;
		reset_option_to_default("tabstop");
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

static void
timeoutlen_handler(OPT_OP op, optval_t val)
{
	if(val.int_val < 0)
	{
		text_buffer_addf("Argument must be >= 0: %d", val.int_val);
		error = 1;
		val.int_val = 0;
		set_option("timeoutlen", val);
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
		set_option("trashdir", val);
	}
	free(expanded_path);
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
	cfg.vi_cmd_bg = ends_with(cfg.vi_command, "&");
	if(cfg.vi_cmd_bg)
		cfg.vi_command[strlen(cfg.vi_command) - 1] = '\0';
}

static void
vixcmd_handler(OPT_OP op, optval_t val)
{
	(void)replace_string(&cfg.vi_x_command, val.str_val);
	cfg.vi_x_cmd_bg = ends_with(cfg.vi_x_command, "&");
	if(cfg.vi_x_cmd_bg)
		cfg.vi_x_command[strlen(cfg.vi_x_command) - 1] = '\0';
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
/* vim: set cinoptions+=t0 : */
