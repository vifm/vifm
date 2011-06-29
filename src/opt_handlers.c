#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "filelist.h"
#include "options.h"
#include "status.h"
#include "ui.h"

#include "opt_handlers.h"

static void add_options(void);
static void load_options(void);
static void print_func(const char *msg, const char *description);
static void fastrun_handler(enum opt_op op, union optval_t val);
static void iec_handler(enum opt_op op, union optval_t val);
static void runexec_handler(enum opt_op op, union optval_t val);
static void sortnumbers_handler(enum opt_op op, union optval_t val);
static void sort_handler(enum opt_op op, union optval_t val);
static void sort_order_handler(enum opt_op op, union optval_t val);
static void timefmt_handler(enum opt_op op, union optval_t val);
static void trash_handler(enum opt_op op, union optval_t val);
static void undolevels_handler(enum opt_op op, union optval_t val);
static void vicmd_handler(enum opt_op op, union optval_t val);
static void vimhelp_handler(enum opt_op op, union optval_t val);
static void wrap_handler(enum opt_op op, union optval_t val);

static int save_msg;

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

void
init_options(void)
{
	set_print_handler(&print_func);
	add_options();
	load_options();
}

static void
add_options(void)
{
	add_option("fastrun", OPT_BOOL, 0, NULL, &fastrun_handler);
	add_option("iec", OPT_BOOL, 0, NULL, &iec_handler);
	add_option("runexec", OPT_BOOL, 0, NULL, &runexec_handler);
	add_option("sortnumbers", OPT_BOOL, 0, NULL, &sortnumbers_handler);
	add_option("timefmt", OPT_STR, 0, NULL, &timefmt_handler);
	add_option("trash", OPT_BOOL, 0, NULL, &trash_handler);
	add_option("undolevels", OPT_INT, 0, NULL, &undolevels_handler);
	add_option("vicmd", OPT_STR, 0, NULL, &vicmd_handler);
	add_option("vimhelp", OPT_BOOL, 0, NULL, &vimhelp_handler);
	add_option("wrap", OPT_BOOL, 0, NULL, &wrap_handler);

	/* local options */
	add_option("sort", OPT_ENUM, NUM_SORT_OPTIONS, sort_enum, &sort_handler);
	add_option("sortorder", OPT_ENUM, 2, sort_order_enum, &sort_order_handler);
}

static void
load_options(void)
{
	union optval_t val;

	val.bool_val = cfg.use_iec_prefixes;
	set_option("iec", val);

	val.bool_val = cfg.auto_execute;
	set_option("runexec", val);

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

	val.bool_val = cfg.use_vim_help;
	set_option("vimhelp", val);

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
	set_options(args);
	return save_msg;
}

static void
print_func(const char *msg, const char *description)
{
	if(*msg == '\0')
	{
		status_bar_message(description);
	}
	else
	{
		char buf[strlen(msg) + 2 + strlen(description) + 1];
		sprintf(buf, "%s: %s", msg, description);
		status_bar_message(buf);
	}
	save_msg = 1;
}

static void
fastrun_handler(enum opt_op op, union optval_t val)
{
	curr_stats.fast_run = val.bool_val;
}

static void
iec_handler(enum opt_op op, union optval_t val)
{
	cfg.use_iec_prefixes = val.bool_val;

	redraw_lists();
}

static void
runexec_handler(enum opt_op op, union optval_t val)
{
	cfg.auto_execute = val.bool_val;
}

static void
sortnumbers_handler(enum opt_op op, union optval_t val)
{
	cfg.sort_numbers = val.bool_val;
	load_saving_pos(curr_view, 1);
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
vimhelp_handler(enum opt_op op, union optval_t val)
{
	cfg.use_vim_help = val.bool_val;
}

static void
wrap_handler(enum opt_op op, union optval_t val)
{
	cfg.wrap_quick_view = val.bool_val;
	if(curr_stats.view)
		quick_view_file(curr_view);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
