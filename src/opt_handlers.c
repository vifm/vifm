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

#include "opt_handlers.h"

static void add_options(void);
static void load_options(void);
static void print_func(const char *msg, const char *description);
static void confirm_handler(enum opt_op op, union optval_t val);
static void fastrun_handler(enum opt_op op, union optval_t val);
static void followlinks_handler(enum opt_op op, union optval_t val);
static void fusehome_handler(enum opt_op op, union optval_t val);
static void history_handler(enum opt_op op, union optval_t val);
static void hlsearch_handler(enum opt_op op, union optval_t val);
static void iec_handler(enum opt_op op, union optval_t val);
static void ignorecase_handler(enum opt_op op, union optval_t val);
static void reversecol_handler(enum opt_op op, union optval_t val);
static void runexec_handler(enum opt_op op, union optval_t val);
static void smartcase_handler(enum opt_op op, union optval_t val);
static void sortnumbers_handler(enum opt_op op, union optval_t val);
static void sort_handler(enum opt_op op, union optval_t val);
static void sort_order_handler(enum opt_op op, union optval_t val);
static void timefmt_handler(enum opt_op op, union optval_t val);
static void trash_handler(enum opt_op op, union optval_t val);
static void undolevels_handler(enum opt_op op, union optval_t val);
static void vicmd_handler(enum opt_op op, union optval_t val);
static void vifminfo_handler(enum opt_op op, union optval_t val);
static void vimhelp_handler(enum opt_op op, union optval_t val);
static void wildmenu_handler(enum opt_op op, union optval_t val);
static void wrap_handler(enum opt_op op, union optval_t val);

static int save_msg;
static char print_buf[320];

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
};

static const char * sort_order_enum[] = {
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
};

void
init_option_handlers(void)
{
	static int opt_changed;
	init_options(&opt_changed, &print_func);
	add_options();
	load_options();
}

static void
add_options(void)
{
	add_option("confirm", "cf", OPT_BOOL, 0, NULL, &confirm_handler);
	add_option("fastrun", "", OPT_BOOL, 0, NULL, &fastrun_handler);
	add_option("followlinks", "", OPT_BOOL, 0, NULL, &followlinks_handler);
	add_option("fusehome", "", OPT_STR, 0, NULL, &fusehome_handler);
	add_option("history", "hi", OPT_INT, 0, NULL, &history_handler);
	add_option("hlsearch", "hls", OPT_BOOL, 0, NULL, &hlsearch_handler);
	add_option("iec", "", OPT_BOOL, 0, NULL, &iec_handler);
	add_option("ignorecase", "ic", OPT_BOOL, 0, NULL, &ignorecase_handler);
	add_option("reversecol", "", OPT_BOOL, 0, NULL, &reversecol_handler);
	add_option("runexec", "", OPT_BOOL, 0, NULL, &runexec_handler);
	add_option("smartcase", "scs", OPT_BOOL, 0, NULL, &smartcase_handler);
	add_option("sortnumbers", "", OPT_BOOL, 0, NULL, &sortnumbers_handler);
	add_option("timefmt", "", OPT_STR, 0, NULL, &timefmt_handler);
	add_option("trash", "", OPT_BOOL, 0, NULL, &trash_handler);
	add_option("undolevels", "ul", OPT_INT, 0, NULL, &undolevels_handler);
	add_option("vicmd", "", OPT_STR, 0, NULL, &vicmd_handler);
	add_option("vifminfo", "", OPT_SET, ARRAY_LEN(vifminfo_set), vifminfo_set,
			&vifminfo_handler);
	add_option("vimhelp", "", OPT_BOOL, 0, NULL, &vimhelp_handler);
	add_option("wildmenu", "wmnu", OPT_BOOL, 0, NULL, &wildmenu_handler);
	add_option("wrap", "", OPT_BOOL, 0, NULL, &wrap_handler);

	/* local options */
	add_option("sort", "", OPT_ENUM, ARRAY_LEN(sort_enum), sort_enum,
			&sort_handler);
	add_option("sortorder", "", OPT_ENUM, 2, sort_order_enum,
			&sort_order_handler);
}

static void
load_options(void)
{
	union optval_t val;

	val.bool_val = cfg.confirm;
	set_option("confirm", val);

	val.bool_val = cfg.fast_run;
	set_option("fastrun", val);

	val.bool_val = cfg.follow_links;
	set_option("followlinks", val);

	val.str_val = cfg.fuse_home;
	set_option("fusehome", val);

	val.int_val = cfg.history_len;
	set_option("history", val);

	val.bool_val = cfg.hl_search;
	set_option("hlsearch", val);

	val.bool_val = cfg.use_iec_prefixes;
	set_option("iec", val);

	val.bool_val = cfg.ignore_case;
	set_option("ignorecase", val);

	val.bool_val = cfg.invert_cur_line;
	set_option("reversecol", val);

	val.bool_val = cfg.auto_execute;
	set_option("runexec", val);

	val.bool_val = cfg.smart_case;
	set_option("smartcase", val);

	val.bool_val = cfg.sort_numbers;
	set_option("sortnumbers", val);

	val.str_val = cfg.time_format + 1;
	set_option("timefmt", val);

	val.bool_val = cfg.use_trash;
	set_option("trash", val);

	val.int_val = cfg.undo_levels;
	set_option("undolevels", val);

	val.str_val = cfg.vi_command;
	set_option("vicmd", val);

	val.set_items = cfg.vifm_info;
	set_option("vifminfo", val);

	val.bool_val = cfg.use_vim_help;
	set_option("vimhelp", val);

	val.bool_val = cfg.wild_menu;
	set_option("wildmenu", val);

	val.bool_val = cfg.wrap_quick_view;
	set_option("wrap", val);
}

void
load_local_options(FileView *view)
{
	union optval_t val;

	val.enum_item = view->sort_type;
	set_option("sort", val);

	val.enum_item = view->sort_descending;
	set_option("sortorder", val);
}

int
process_set_args(const char *args)
{
	save_msg = 0;
	print_buf[0] = '\0';
	if(set_options(args) != 0)
		print_func("", "Invalid argument for :set command");
	status_bar_message(print_buf);
	return save_msg;
}

static void
print_func(const char *msg, const char *description)
{
	if(print_buf[0] != '\0')
	{
		strncat(print_buf, "\n", sizeof(print_buf));
		print_buf[sizeof(print_buf) - 1] = '\0';
	}
	if(*msg == '\0')
	{
		strncat(print_buf, description, sizeof(print_buf));
		print_buf[sizeof(print_buf) - 1] = '\0';
	}
	else
	{
		strncat(print_buf, msg, sizeof(print_buf));
		print_buf[sizeof(print_buf) - 1] = '\0';
		strncat(print_buf, ": ", sizeof(print_buf));
		print_buf[sizeof(print_buf) - 1] = '\0';
		strncat(print_buf, description, sizeof(print_buf));
		print_buf[sizeof(print_buf) - 1] = '\0';
	}
	save_msg = 1;
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
	cfg.fuse_home = strdup(val.str_val);
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
	if(val.int_val <= 0)
	{
		free_view_history(&lwin);
		free_view_history(&rwin);

		cfg.history_len = val.int_val;
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

		save_view_history(&lwin);
		save_view_history(&rwin);
	}
	else
	{
		cfg.history_len = val.int_val;
	}
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
reversecol_handler(enum opt_op op, union optval_t val)
{
	cfg.invert_cur_line = val.bool_val;
	redraw_lists();
}

static void
runexec_handler(enum opt_op op, union optval_t val)
{
	cfg.auto_execute = val.bool_val;
}

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
	change_sort_type(curr_view, val.enum_item, curr_view->sort_descending);
}

static void
sort_order_handler(enum opt_op op, union optval_t val)
{
	change_sort_type(curr_view, curr_view->sort_type, val.enum_item);
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
	free(cfg.vi_command);
	cfg.vi_command = strdup(val.str_val);
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
