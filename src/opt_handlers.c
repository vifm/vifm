#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "filelist.h"
#include "options.h"
#include "ui.h"

#include "opt_handlers.h"

static void add_options(void);
static void load_options(void);
static void print_func(const char *msg, const char *description);
static void iec_handler(enum opt_op op, union optval_t val);
static void sort_handler(enum opt_op op, union optval_t val);
static void sort_order_handler(enum opt_op op, union optval_t val);
static void timefmt_handler(enum opt_op op, union optval_t val);
static void vicmd_handler(enum opt_op op, union optval_t val);

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
	add_option("iec", OPT_BOOL, 0, NULL, &iec_handler);
	add_option("timefmt", OPT_STR, 0, NULL, &timefmt_handler);
	add_option("vicmd", OPT_STR, 0, NULL, &vicmd_handler);

	/* local options */
	add_option("sort", OPT_ENUM, NUM_SORT_OPTIONS, sort_enum, &sort_handler);
	add_option("sortorder", OPT_ENUM, 2, sort_order_enum, &sort_order_handler);
}

static void
load_options(void)
{
	union optval_t val;

	val.int_val = cfg.use_iec_prefixes;
	set_option("iec", val);

	val.str_val = cfg.time_format + 1;
	set_option("timefmt", val);

	val.str_val = cfg.vi_command;
	set_option("vicmd", val);
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
iec_handler(enum opt_op op, union optval_t val)
{
	cfg.use_iec_prefixes = val.int_val;

	redraw_lists();
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
vicmd_handler(enum opt_op op, union optval_t val)
{
	free(cfg.vi_command);
	cfg.vi_command = strdup(val.str_val);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
