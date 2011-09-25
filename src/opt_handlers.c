#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "color_scheme.h"
#include "config.h"
#include "filelist.h"
#include "macros.h"
#include "options.h"
#include "status.h"
#include "ui.h"
#include "utils.h"

#include "opt_handlers.h"

static void load_options_defaults(void);
static void add_options(void);
static void print_func(const char *msg, const char *description);
static void autochpos_handler(enum opt_op op, union optval_t val);
static void confirm_handler(enum opt_op op, union optval_t val);
static void fastrun_handler(enum opt_op op, union optval_t val);
static void followlinks_handler(enum opt_op op, union optval_t val);
static void fusehome_handler(enum opt_op op, union optval_t val);
static void gdefault_handler(enum opt_op op, union optval_t val);
static void history_handler(enum opt_op op, union optval_t val);
static void hlsearch_handler(enum opt_op op, union optval_t val);
static void iec_handler(enum opt_op op, union optval_t val);
static void ignorecase_handler(enum opt_op op, union optval_t val);
static void runexec_handler(enum opt_op op, union optval_t val);
static void scrolloff_handler(enum opt_op op, union optval_t val);
static void shell_handler(enum opt_op op, union optval_t val);
#ifndef _WIN32
static void slowfs_handler(enum opt_op op, union optval_t val);
#endif
static void smartcase_handler(enum opt_op op, union optval_t val);
static void sortnumbers_handler(enum opt_op op, union optval_t val);
static void sort_handler(enum opt_op op, union optval_t val);
static void sortorder_handler(enum opt_op op, union optval_t val);
static void timefmt_handler(enum opt_op op, union optval_t val);
static void timeoutlen_handler(enum opt_op op, union optval_t val);
static void trash_handler(enum opt_op op, union optval_t val);
static void undolevels_handler(enum opt_op op, union optval_t val);
static void vicmd_handler(enum opt_op op, union optval_t val);
static void vixcmd_handler(enum opt_op op, union optval_t val);
static void vifminfo_handler(enum opt_op op, union optval_t val);
static void vimhelp_handler(enum opt_op op, union optval_t val);
static void wildmenu_handler(enum opt_op op, union optval_t val);
static void wrap_handler(enum opt_op op, union optval_t val);

static int save_msg;
static char print_buf[320*80];

static const char * sort_enum[] = {
	"ext",
	"name",
	"gid",
	"gname",
	"mode",
	"uid",
	"uname",
	"size",
	"atime",
	"ctime",
	"mtime",
	"iname",
};

static const char * sort_types[] = {
	"ext",   "+ext",   "-ext",
	"name",  "+name",  "-name",
	"gid",   "+gid",   "-gid",
	"gname", "+gname", "-gname",
	"mode",  "+mode",  "-mode",
	"uid",   "+uid",   "-uid",
	"uname", "+uname", "-uname",
	"size",  "+size",  "-size",
	"atime", "+atime", "-atime",
	"ctime", "+ctime", "-ctime",
	"mtime", "+mtime", "-mtime",
	"iname", "+iname", "-iname",
};

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
};

static struct {
	const char *name;
	const char *abbr;
	enum opt_type type;
	int val_count;
	const char **vals;
	opt_handler handler;
	union optval_t val;
} options[] = {
	/* global options */
	{ "autochpos",   "",     OPT_BOOL,    0,                          NULL,            &autochpos_handler,   },
	{ "confirm",     "cf",   OPT_BOOL,    0,                          NULL,            &confirm_handler,     },
	{ "fastrun",     "",     OPT_BOOL,    0,                          NULL,            &fastrun_handler,     },
	{ "followlinks", "",     OPT_BOOL,    0,                          NULL,            &followlinks_handler, },
	{ "fusehome",    "",     OPT_STR,     0,                          NULL,            &fusehome_handler,    },
	{ "gdefault",    "gd",   OPT_BOOL,    0,                          NULL,            &gdefault_handler,    },
	{ "history",     "hi",   OPT_INT,     0,                          NULL,            &history_handler,     },
	{ "hlsearch",    "hls",  OPT_BOOL,    0,                          NULL,            &hlsearch_handler,    },
	{ "iec",         "",     OPT_BOOL,    0,                          NULL,            &iec_handler,         },
	{ "ignorecase",  "ic",   OPT_BOOL,    0,                          NULL,            &ignorecase_handler,  },
	{ "runexec",     "",     OPT_BOOL,    0,                          NULL,            &runexec_handler,     },
	{ "scrolloff",   "so",   OPT_INT,     0,                          NULL,            &scrolloff_handler,   },
	{ "shell",       "sh",   OPT_STR,     0,                          NULL,            &shell_handler,       },
#ifndef _WIN32
	{ "slowfs",      "",     OPT_STRLIST, 0,                          NULL,            &slowfs_handler,      },
#endif
	{ "smartcase",   "scs",  OPT_BOOL,    0,                          NULL,            &smartcase_handler,   },
	{ "sortnumbers", "",     OPT_BOOL,    0,                          NULL,            &sortnumbers_handler, },
	{ "timefmt",     "",     OPT_STR,     0,                          NULL,            &timefmt_handler,     },
	{ "timeoutlen",  "tm",   OPT_INT,     0,                          NULL,            &timeoutlen_handler,  },
	{ "trash",       "",     OPT_BOOL,    0,                          NULL,            &trash_handler,       },
	{ "undolevels",  "ul",   OPT_INT,     0,                          NULL,            &undolevels_handler,  },
	{ "vicmd",       "",     OPT_STR,     0,                          NULL,            &vicmd_handler,       },
	{ "vixcmd",      "",     OPT_STR,     0,                          NULL,            &vixcmd_handler,      },
	{ "vifminfo",    "",     OPT_SET,     ARRAY_LEN(vifminfo_set),    vifminfo_set,    &vifminfo_handler,    },
	{ "vimhelp",     "",     OPT_BOOL,    0,                          NULL,            &vimhelp_handler,     },
	{ "wildmenu",    "wmnu", OPT_BOOL,    0,                          NULL,            &wildmenu_handler,    },
	{ "wrap",        "",     OPT_BOOL,    0,                          NULL,            &wrap_handler,        },
	/* local options */
	{ "sort",        "",     OPT_STRLIST, ARRAY_LEN(sort_types),      sort_types,      &sort_handler,        },
	{ "sortorder",   "",     OPT_ENUM,    ARRAY_LEN(sortorder_enum),  sortorder_enum,  &sortorder_handler,   },
};

void
init_option_handlers(void)
{
	static int opt_changed;
	init_options(&opt_changed, &print_func);
	load_options_defaults();
	add_options();
}

static void
load_options_defaults(void)
{
	int i = 0;

	/* global options */
	options[i++].val.bool_val = cfg.auto_ch_pos;
	options[i++].val.bool_val = cfg.confirm;
	options[i++].val.bool_val = cfg.fast_run;
	options[i++].val.bool_val = cfg.follow_links;
	options[i++].val.str_val = cfg.fuse_home;
	options[i++].val.bool_val = cfg.gdefault;
	options[i++].val.int_val = cfg.history_len;
	options[i++].val.bool_val = cfg.hl_search;
	options[i++].val.bool_val = cfg.use_iec_prefixes;
	options[i++].val.bool_val = cfg.ignore_case;
	options[i++].val.bool_val = cfg.auto_execute;
	options[i++].val.int_val = cfg.scroll_off;
	options[i++].val.str_val = cfg.shell;
#ifndef _WIN32
	options[i++].val.str_val = cfg.slow_fs_list;
#endif
	options[i++].val.bool_val = cfg.smart_case;
	options[i++].val.bool_val = cfg.sort_numbers;
	options[i++].val.str_val = cfg.time_format + 1;
	options[i++].val.int_val = cfg.timeout_len;
	options[i++].val.bool_val = cfg.use_trash;
	options[i++].val.int_val = cfg.undo_levels;
	options[i++].val.str_val = cfg.vi_command;
	options[i++].val.str_val = cfg.vi_x_command;
	options[i++].val.set_items = cfg.vifm_info;
	options[i++].val.bool_val = cfg.use_vim_help;
	options[i++].val.bool_val = cfg.wild_menu;
	options[i++].val.bool_val = cfg.wrap_quick_view;

	/* local options */
	options[i++].val.str_val = "+name";
	options[i++].val.enum_item = 0;

	assert(ARRAY_LEN(options) == i);
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
	load_sort(view);
}

void
load_sort(FileView *view)
{
	union optval_t val;
	char buf[64] = "";
	int j, i;

	for(j = 0; j < NUM_SORT_OPTIONS && view->sort[j] <= NUM_SORT_OPTIONS; j++)
		if(abs(view->sort[j]) == SORT_BY_NAME ||
				abs(view->sort[j]) == SORT_BY_INAME)
			break;
	if(j < NUM_SORT_OPTIONS && view->sort[j] > NUM_SORT_OPTIONS)
#ifndef _WIN32
		view->sort[j++] = SORT_BY_NAME;
#else
		view->sort[j++] = SORT_BY_INAME;
#endif

	i = -1;
	while(++i < NUM_SORT_OPTIONS && view->sort[i] <= NUM_SORT_OPTIONS)
	{
		if(buf[0] != '\0')
			strcat(buf, ",");
		if(view->sort[i] < 0)
			strcat(buf, "-");
		else
			strcat(buf, "+");
		strcat(buf, sort_enum[abs(view->sort[i]) - 1]);
	}

	val.str_val = buf;
	set_option("sort", val);

	val.enum_item = (view->sort[0] < 0);
	set_option("sortorder", val);
}

int
process_set_args(const char *args)
{
	save_msg = 0;
	print_buf[0] = '\0';
	if(set_options(args) != 0) /* changes print_buf and save_msg */
	{
		print_func("", "Invalid argument for :set command");
		status_bar_error(print_buf);
		save_msg = -1;
	}
	else if(print_buf[0] != '\0')
	{
		status_bar_message(print_buf);
	}
	return save_msg;
}

static void
print_func(const char *msg, const char *description)
{
	if(print_buf[0] != '\0')
	{
		strncat(print_buf, "\n", sizeof(print_buf) - strlen(print_buf) - 1);
	}
	if(*msg == '\0')
	{
		strncat(print_buf, description, sizeof(print_buf) - strlen(print_buf) - 1);
	}
	else
	{
		snprintf(print_buf, sizeof(print_buf) - strlen(print_buf), "%s: %s", msg,
				description);
	}
	save_msg = 1;
}

static void
autochpos_handler(enum opt_op op, union optval_t val)
{
	cfg.auto_ch_pos = val.bool_val;
	if(curr_stats.vifm_started < 2)
	{
		clean_positions_in_history(curr_view);
		clean_positions_in_history(other_view);
	}
}

static void
confirm_handler(enum opt_op op, union optval_t val)
{
	cfg.confirm = val.bool_val;
}

static void
fastrun_handler(enum opt_op op, union optval_t val)
{
	cfg.fast_run = val.bool_val;
}

static void
followlinks_handler(enum opt_op op, union optval_t val)
{
	cfg.follow_links = val.bool_val;
}

static void
fusehome_handler(enum opt_op op, union optval_t val)
{
	free(cfg.fuse_home);
	cfg.fuse_home = expand_tilde(strdup(val.str_val));
}

static void
gdefault_handler(enum opt_op op, union optval_t val)
{
	cfg.gdefault = val.bool_val;
}

static void
reduce_view_history(FileView *view, int size)
{
	int i;
	int delta;

	delta = MIN(view->history_num - size, view->history_pos);
	if(delta <= 0)
		return;

	for(i = 0; i < size && i + delta <= view->history_num; i++)
	{
		strncpy(view->history[i].file, view->history[i + delta].file,
				sizeof(view->history[i].file));
		strncpy(view->history[i].dir, view->history[i + delta].dir,
				sizeof(view->history[i].dir));
	}

	if(view->history_num >= size)
		view->history_num = size - 1;
	view->history_pos -= delta;
}

static void
free_view_history(FileView *view)
{
	free(view->history);
	view->history = NULL;
	view->history_num = 0;
	view->history_pos = 0;
}

static void
history_handler(enum opt_op op, union optval_t val)
{
	int delta;

	if(val.int_val <= 0)
	{
		free_view_history(&lwin);
		free_view_history(&rwin);

		cfg.history_len = val.int_val;
		cfg.cmd_history_len = val.int_val;
		cfg.prompt_history_len = val.int_val;
		cfg.search_history_len = val.int_val;

		cfg.cmd_history_num = -1;
		cfg.prompt_history_num = -1;
		cfg.search_history_num = -1;
		return;
	}

	if(cfg.history_len > val.int_val)
	{
		reduce_view_history(&lwin, val.int_val);
		reduce_view_history(&rwin, val.int_val);
	}

	lwin.history = realloc(lwin.history, sizeof(history_t)*val.int_val);
	rwin.history = realloc(rwin.history, sizeof(history_t)*val.int_val);

	if(cfg.history_len <= 0)
	{
		cfg.history_len = val.int_val;

		save_view_history(&lwin, NULL, NULL, -1);
		save_view_history(&rwin, NULL, NULL, -1);
	}
	else
	{
		cfg.history_len = val.int_val;
	}

	delta = val.int_val - cfg.cmd_history_len;
	if(delta < 0 && cfg.cmd_history_len > 0)
	{
		int i;
		for(i = cfg.cmd_history_len; i < val.int_val; i++)
			free(cfg.cmd_history[i]);
		for(i = cfg.cmd_history_len; i < val.int_val; i++)
			free(cfg.prompt_history[i]);
		for(i = cfg.cmd_history_len; i < val.int_val; i++)
			free(cfg.search_history[i]);
	}
	cfg.cmd_history = realloc(cfg.cmd_history, val.int_val*sizeof(char *));
	cfg.prompt_history = realloc(cfg.prompt_history, val.int_val*sizeof(char *));
	cfg.search_history = realloc(cfg.search_history, val.int_val*sizeof(char *));
	if(delta > 0)
	{
		int i;
		for(i = val.int_val; i < cfg.cmd_history_len; i++)
			cfg.cmd_history[i] = NULL;
		for(i = val.int_val; i < cfg.cmd_history_len; i++)
			cfg.prompt_history[i] = NULL;
		for(i = val.int_val; i < cfg.cmd_history_len; i++)
			cfg.search_history[i] = NULL;
	}

	cfg.cmd_history_len = val.int_val;
	cfg.prompt_history_len = val.int_val;
	cfg.search_history_len = val.int_val;
}

static void
hlsearch_handler(enum opt_op op, union optval_t val)
{
	cfg.hl_search = val.bool_val;
}

static void
iec_handler(enum opt_op op, union optval_t val)
{
	cfg.use_iec_prefixes = val.bool_val;

	redraw_lists();
}

static void
ignorecase_handler(enum opt_op op, union optval_t val)
{
	cfg.ignore_case = val.bool_val;
}

static void
scrolloff_handler(enum opt_op op, union optval_t val)
{
	cfg.scroll_off = val.int_val;
	if(cfg.scroll_off > 0)
		move_to_list_pos(curr_view, curr_view->list_pos);
}

static void
runexec_handler(enum opt_op op, union optval_t val)
{
	cfg.auto_execute = val.bool_val;
}

static void
shell_handler(enum opt_op op, union optval_t val)
{
	char *s;
	if((s = strdup(val.str_val)) == NULL)
		return;
	free(cfg.shell);
	cfg.shell = s;
}

#ifndef _WIN32
static void
slowfs_handler(enum opt_op op, union optval_t val)
{
	char *s;
	if((s = strdup(val.str_val)) == NULL)
		return;
	free(cfg.slow_fs_list);
	cfg.slow_fs_list = s;
}
#endif

static void
smartcase_handler(enum opt_op op, union optval_t val)
{
	cfg.smart_case = val.bool_val;
}

static void
sortnumbers_handler(enum opt_op op, union optval_t val)
{
	cfg.sort_numbers = val.bool_val;
	load_saving_pos(curr_view, 1);
	load_dir_list(other_view, 1);
}

static void
sort_handler(enum opt_op op, union optval_t val)
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
	} while(*p != '\0');

	for(j = 0; j < i; j++)
		if(abs(curr_view->sort[j]) == SORT_BY_NAME ||
				abs(curr_view->sort[j]) == SORT_BY_INAME)
			break;
	if(j == i)
#ifndef _WIN32
		curr_view->sort[i++] = SORT_BY_NAME;
#else
		curr_view->sort[i++] = SORT_BY_INAME;
#endif
	while(i < NUM_SORT_OPTIONS)
		curr_view->sort[i++] = NUM_SORT_OPTIONS + 1;
	load_saving_pos(curr_view, 1);

	load_sort(curr_view);
}

static void
sortorder_handler(enum opt_op op, union optval_t val)
{
	if((val.enum_item ? -1 : +1)*curr_view->sort[0] < 0)
	{
		curr_view->sort[0] = -curr_view->sort[0];
		load_saving_pos(curr_view, 1);
		load_sort(curr_view);
	}
}

static void
timefmt_handler(enum opt_op op, union optval_t val)
{
	free(cfg.time_format);
	cfg.time_format = malloc(1 + strlen(val.str_val) + 1);
	strcpy(cfg.time_format, " ");
	strcat(cfg.time_format, val.str_val);

	redraw_lists();
}

static void
timeoutlen_handler(enum opt_op op, union optval_t val)
{
	cfg.timeout_len = val.int_val;
}

static void
trash_handler(enum opt_op op, union optval_t val)
{
	cfg.use_trash = val.bool_val;
}

static void
undolevels_handler(enum opt_op op, union optval_t val)
{
	cfg.undo_levels = val.int_val;
}

static void
vicmd_handler(enum opt_op op, union optval_t val)
{
	size_t len;

	free(cfg.vi_command);
	cfg.vi_command = strdup(val.str_val);
	len = strlen(cfg.vi_command);
	cfg.vi_cmd_bg = (len > 1 && cfg.vi_command[len - 1] == '&');
	if(cfg.vi_cmd_bg)
		cfg.vi_command[len - 1] = '\0';
}

static void
vixcmd_handler(enum opt_op op, union optval_t val)
{
	size_t len;

	free(cfg.vi_x_command);
	cfg.vi_x_command = strdup(val.str_val);
	len = strlen(cfg.vi_x_command);
	cfg.vi_x_cmd_bg = (len > 1 && cfg.vi_x_command[len - 1] == '&');
	if(cfg.vi_x_cmd_bg)
		cfg.vi_x_command[len - 1] = '\0';
}

static void
vifminfo_handler(enum opt_op op, union optval_t val)
{
	cfg.vifm_info = val.set_items;
}

static void
vimhelp_handler(enum opt_op op, union optval_t val)
{
	cfg.use_vim_help = val.bool_val;
}

static void
wildmenu_handler(enum opt_op op, union optval_t val)
{
	cfg.wild_menu = val.bool_val;
}

static void
wrap_handler(enum opt_op op, union optval_t val)
{
	cfg.wrap_quick_view = val.bool_val;
	if(curr_stats.view)
		quick_view_file(curr_view);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
